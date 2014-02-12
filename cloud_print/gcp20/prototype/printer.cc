// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/printer.h"

#include <algorithm>
#include <limits.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cloud_print/gcp20/prototype/command_line_reader.h"
#include "cloud_print/gcp20/prototype/local_settings.h"
#include "cloud_print/gcp20/prototype/service_parameters.h"
#include "cloud_print/gcp20/prototype/special_io.h"
#include "cloud_print/version.h"
#include "net/base/net_util.h"
#include "net/base/url_util.h"

const char kPrinterStatePathDefault[] = "printer_state.json";

namespace {

const uint16 kHttpPortDefault = 10101;
const uint32 kTtlDefault = 60*60;  // in seconds

const char kServiceType[] = "_privet._tcp.local";
const char kSecondaryServiceType[] = "_printer._sub._privet._tcp.local";
const char kServiceNamePrefixDefault[] = "gcp20_device_";
const char kServiceDomainNameFormatDefault[] = "my-privet-device%d.local";

const char kPrinterName[] = "Google GCP2.0 Prototype";
const char kPrinterDescription[] = "Printer emulator";

const char kUserConfirmationTitle[] = "Confirm registration: type 'y' if you "
                                      "agree and any other to discard\n";
const int kUserConfirmationTimeout = 30;  // in seconds
const int kRegistrationTimeout = 60;  // in seconds
const int kReconnectTimeout = 5;  // in seconds

const double kTimeToNextAccessTokenUpdate = 0.8;  // relatively to living time.

const char kCdd[] =
"{"
"  'version': '1.0',"
"  'printer': {"
"    'supported_content_type': ["
"      {"
"        'content_type': 'application/pdf'"
"      },"
"      {"
"        'content_type': 'image/pwg-raster'"
"      },"
"      {"
"        'content_type': 'image/jpeg'"
"      }"
"    ],"
"    'color': {"
"     'option': ["
"        {"
"          'is_default': true,"
"          'type': 'STANDARD_COLOR'"
"        },"
"        {"
"          'type': 'STANDARD_MONOCHROME'"
"        }"
"      ]"
"    },"
"    'media_size': {"
"       'option': [ {"
"          'height_microns': 297000,"
"          'name': 'ISO_A4',"
"          'width_microns': 210000"
"       }, {"
"          'custom_display_name': 'Letter',"
"          'height_microns': 279400,"
"          'is_default': true,"
"          'name': 'NA_LETTER',"
"          'width_microns': 215900"
"       } ]"
"    },"
"    'page_orientation': {"
"       'option': [ {"
"          'is_default': true,"
"          'type': 'PORTRAIT'"
"       }, {"
"          'type': 'LANDSCAPE'"
"       } ]"
"    },"
"    'reverse_order': {"
"      'default': false"
"    }"
"  }"
"}";

// Returns local IP address number of first interface found (except loopback).
// Return value is empty if no interface found. Possible interfaces names are
// "eth0", "wlan0" etc. If interface name is empty, function will return IP
// address of first interface found.
net::IPAddressNumber GetLocalIp(const std::string& interface_name,
                                bool return_ipv6_number) {
  net::NetworkInterfaceList interfaces;
  bool success = net::GetNetworkList(
      &interfaces, net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES);
  DCHECK(success);

  size_t expected_address_size = return_ipv6_number ? net::kIPv6AddressSize
                                                    : net::kIPv4AddressSize;

  for (net::NetworkInterfaceList::iterator iter = interfaces.begin();
       iter != interfaces.end(); ++iter) {
    if (iter->address.size() == expected_address_size &&
        (interface_name.empty() || interface_name == iter->name)) {
      return iter->address;
    }
  }

  return net::IPAddressNumber();
}

std::string GetDescription() {
  std::string result = kPrinterDescription;
  net::IPAddressNumber ip = GetLocalIp("", false);
  if (!ip.empty())
    result += " at " + net::IPAddressToString(ip);
  return result;
}

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
  return base::MessageLoop::current()->message_loop_proxy();
}

}  // namespace

using cloud_print_response_parser::Job;

Printer::Printer()
    : connection_state_(OFFLINE),
      http_server_(this),
      on_idle_posted_(false),
      pending_local_settings_check_(false),
      pending_print_jobs_check_(false),
      pending_deletion_(false) {
}

Printer::~Printer() {
  Stop();
}

bool Printer::Start() {
  if (IsRunning())
    return true;

  LoadFromFile();

  if (state_.local_settings.local_discovery && !StartLocalDiscoveryServers())
    return false;

  print_job_handler_.reset(new PrintJobHandler);
  xtoken_ = XPrivetToken();
  starttime_ = base::Time::Now();

  TryConnect();
  return true;
}

bool Printer::IsRunning() const {
  return print_job_handler_;
}

void Printer::Stop() {
  if (!IsRunning())
    return;
  dns_server_.Shutdown();
  http_server_.Shutdown();
  requester_.reset();
  print_job_handler_.reset();
  xmpp_listener_.reset();
}

std::string Printer::GetRawCdd() {
  std::string json_str;
  base::JSONWriter::WriteWithOptions(&GetCapabilities(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json_str);
  return json_str;
}

void Printer::OnAuthError() {
  LOG(ERROR) << "Auth error occurred";
  state_.access_token_update = base::Time();
  FallOffline(true);
}

std::string Printer::GetAccessToken() {
  return state_.access_token;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationStart(
    const std::string& user) {
  CheckRegistrationExpiration();

  PrinterState::ConfirmationState conf_state = state_.confirmation_state;
  if (state_.registration_state == PrinterState::REGISTRATION_ERROR ||
      conf_state == PrinterState::CONFIRMATION_TIMEOUT ||
      conf_state == PrinterState::CONFIRMATION_DISCARDED) {
    state_ = PrinterState();
  }

  PrivetHttpServer::RegistrationErrorStatus status = CheckCommonRegErrors(user);
  if (status != PrivetHttpServer::REG_ERROR_OK)
    return status;

  if (state_.registration_state != PrinterState::UNREGISTERED)
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;

  UpdateRegistrationExpiration();

  state_ = PrinterState();
  state_.user = user;
  state_.registration_state = PrinterState::REGISTRATION_STARTED;

  if (CommandLine::ForCurrentProcess()->HasSwitch("disable-confirmation")) {
    state_.confirmation_state = PrinterState::CONFIRMATION_CONFIRMED;
    VLOG(0) << "Registration confirmed by default.";
  } else {
    LOG(WARNING) << kUserConfirmationTitle;
    base::Time valid_until = base::Time::Now() +
        base::TimeDelta::FromSeconds(kUserConfirmationTimeout);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&Printer::WaitUserConfirmation, AsWeakPtr(), valid_until));
  }

  requester_->StartRegistration(GenerateProxyId(), kPrinterName, user,
                                state_.local_settings, GetRawCdd());

  return PrivetHttpServer::REG_ERROR_OK;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationGetClaimToken(
    const std::string& user,
    std::string* token,
    std::string* claim_url) {
  PrivetHttpServer::RegistrationErrorStatus status = CheckCommonRegErrors(user);
  if (status != PrivetHttpServer::REG_ERROR_OK)
    return status;

  // Check if |action=start| was called, but |action=complete| wasn't.
  if (state_.registration_state != PrinterState::REGISTRATION_STARTED &&
      state_.registration_state !=
          PrinterState::REGISTRATION_CLAIM_TOKEN_READY) {
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;
  }

  // If |action=getClaimToken| is valid in this state (was checked above) then
  // check confirmation status.
  if (state_.confirmation_state != PrinterState::CONFIRMATION_CONFIRMED)
    return ConfirmationToRegistrationError(state_.confirmation_state);

  UpdateRegistrationExpiration();

  // If reply wasn't received yet, reply with |pending_user_action| error.
  if (state_.registration_state == PrinterState::REGISTRATION_STARTED)
    return PrivetHttpServer::REG_ERROR_PENDING_USER_ACTION;

  DCHECK_EQ(state_.confirmation_state, PrinterState::CONFIRMATION_CONFIRMED);
  DCHECK_EQ(state_.registration_state,
            PrinterState::REGISTRATION_CLAIM_TOKEN_READY);

  *token = state_.registration_token;
  *claim_url = state_.complete_invite_url;
  return PrivetHttpServer::REG_ERROR_OK;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationComplete(
    const std::string& user,
    std::string* device_id) {
  PrivetHttpServer::RegistrationErrorStatus status = CheckCommonRegErrors(user);
  if (status != PrivetHttpServer::REG_ERROR_OK)
    return status;

  if (state_.registration_state != PrinterState::REGISTRATION_CLAIM_TOKEN_READY)
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;

  UpdateRegistrationExpiration();

  if (state_.confirmation_state != PrinterState::CONFIRMATION_CONFIRMED)
    return ConfirmationToRegistrationError(state_.confirmation_state);

  state_.registration_state = PrinterState::REGISTRATION_COMPLETING;
  requester_->CompleteRegistration();
  *device_id = state_.device_id;

  return PrivetHttpServer::REG_ERROR_OK;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationCancel(
    const std::string& user) {
  PrivetHttpServer::RegistrationErrorStatus status = CheckCommonRegErrors(user);
  if (status != PrivetHttpServer::REG_ERROR_OK &&
      status != PrivetHttpServer::REG_ERROR_SERVER_ERROR) {
    return status;
  }

  if (state_.registration_state == PrinterState::UNREGISTERED)
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;

  InvalidateRegistrationExpiration();

  state_ = PrinterState();

  requester_.reset(new CloudPrintRequester(GetTaskRunner(), this));

  return PrivetHttpServer::REG_ERROR_OK;
}

void Printer::GetRegistrationServerError(std::string* description) {
  DCHECK_EQ(state_.registration_state, PrinterState::REGISTRATION_ERROR)
      << "Method shouldn't be called when not needed.";

  *description = state_.error_description;
}

void Printer::CreateInfo(PrivetHttpServer::DeviceInfo* info) {
  CheckRegistrationExpiration();

  // TODO(maksymb): Replace "text" with constants.
  *info = PrivetHttpServer::DeviceInfo();
  info->version = "1.0";
  info->name = kPrinterName;
  info->description = GetDescription();
  info->url = kCloudPrintUrl;
  info->id = state_.device_id;
  info->device_state = "idle";
  info->connection_state = ConnectionStateToString(connection_state_);
  info->manufacturer = COMPANY_FULLNAME_STRING;
  info->model = "Prototype r" + std::string(LASTCHANGE_STRING);
  info->serial_number = "20CB5FF2-B28C-4EFA-8DCD-516CFF0455A2";
  info->firmware = CHROME_VERSION_STRING;
  info->uptime = static_cast<int>((base::Time::Now() - starttime_).InSeconds());

  info->x_privet_token = xtoken_.GenerateXToken();

  // TODO(maksymb): Create enum for available APIs and replace
  // this API text names with constants from enum. API text names should be only
  // known in PrivetHttpServer.
  if (!IsRegistered()) {
    info->api.push_back("/privet/register");
  } else {
    info->api.push_back("/privet/capabilities");
    if (IsLocalPrintingAllowed()) {
      info->api.push_back("/privet/printer/createjob");
      info->api.push_back("/privet/printer/submitdoc");
      info->api.push_back("/privet/printer/jobstate");
    }
  }

  info->type.push_back("printer");
}

bool Printer::IsRegistered() const {
  return state_.registration_state == PrinterState::REGISTERED;
}

bool Printer::IsLocalPrintingAllowed() const {
  return state_.local_settings.local_printing_enabled;
}

bool Printer::CheckXPrivetTokenHeader(const std::string& token) const {
  return xtoken_.CheckValidXToken(token);
}

const base::DictionaryValue& Printer::GetCapabilities() {
  if (!state_.cdd.get()) {
    std::string cdd_string;
    base::ReplaceChars(kCdd, "'", "\"", &cdd_string);
    scoped_ptr<base::Value> json_val(base::JSONReader::Read(cdd_string));
    base::DictionaryValue* json = NULL;
    CHECK(json_val->GetAsDictionary(&json));
    state_.cdd.reset(json->DeepCopy());
  }
  return *state_.cdd;
}

LocalPrintJob::CreateResult Printer::CreateJob(const std::string& ticket,
                                               std::string* job_id,
                                               int* expires_in,
                                               int* error_timeout,
                                               std::string* error_description) {
  return print_job_handler_->CreatePrintJob(ticket, job_id, expires_in,
                                            error_timeout, error_description);
}

LocalPrintJob::SaveResult Printer::SubmitDoc(const LocalPrintJob& job,
                                             std::string* job_id,
                                             int* expires_in,
                                             std::string* error_description,
                                             int* timeout) {
  return print_job_handler_->SaveLocalPrintJob(job, job_id, expires_in,
                                               error_description, timeout);
}

LocalPrintJob::SaveResult Printer::SubmitDocWithId(
    const LocalPrintJob& job,
    const std::string& job_id,
    int* expires_in,
    std::string* error_description,
    int* timeout) {
  return print_job_handler_->CompleteLocalPrintJob(job, job_id, expires_in,
                                                   error_description, timeout);
}

bool Printer::GetJobState(const std::string& id, LocalPrintJob::Info* info) {
  return print_job_handler_->GetJobState(id, info);
}

void Printer::OnRegistrationStartResponseParsed(
    const std::string& registration_token,
    const std::string& complete_invite_url,
    const std::string& device_id) {
  state_.registration_state = PrinterState::REGISTRATION_CLAIM_TOKEN_READY;
  state_.device_id = device_id;
  state_.registration_token = registration_token;
  state_.complete_invite_url = complete_invite_url;
}

void Printer::OnRegistrationFinished(const std::string& refresh_token,
                                     const std::string& access_token,
                                     int access_token_expires_in_seconds) {
  InvalidateRegistrationExpiration();

  state_.registration_state = PrinterState::REGISTERED;
  state_.refresh_token = refresh_token;
  RememberAccessToken(access_token, access_token_expires_in_seconds);
  TryConnect();
}

void Printer::OnAccesstokenReceviced(const std::string& access_token,
                                     int expires_in_seconds) {
  VLOG(3) << "Function: " << __FUNCTION__;
  RememberAccessToken(access_token, expires_in_seconds);
  switch (connection_state_) {
    case ONLINE:
      PostOnIdle();
      break;

    case CONNECTING:
      TryConnect();
      break;

    default:
      NOTREACHED();
  }
}

void Printer::OnXmppJidReceived(const std::string& xmpp_jid) {
  state_.xmpp_jid = xmpp_jid;
}

void Printer::OnRegistrationError(const std::string& description) {
  LOG(ERROR) << "server_error: " << description;

  SetRegistrationError(description);
}

void Printer::OnNetworkError() {
  VLOG(3) << "Function: " << __FUNCTION__;
  FallOffline(false);
}

void Printer::OnServerError(const std::string& description) {
  VLOG(3) << "Function: " << __FUNCTION__;
  LOG(ERROR) << "Server error: " << description;
  FallOffline(false);
}

void Printer::OnPrintJobsAvailable(const std::vector<Job>& jobs) {
  VLOG(3) << "Function: " << __FUNCTION__;

  VLOG(0) << "Available printjobs: " << jobs.size();
  if (jobs.empty()) {
    pending_print_jobs_check_ = false;
    PostOnIdle();
    return;
  }

  VLOG(0) << "Downloading printjob.";
  requester_->RequestPrintJob(jobs[0]);
  return;
}

void Printer::OnPrintJobDownloaded(const Job& job) {
  VLOG(3) << "Function: " << __FUNCTION__;
  print_job_handler_->SavePrintJob(job.file, job.ticket, job.create_time,
                                   job.job_id, job.title, "pdf");
  requester_->SendPrintJobDone(job.job_id);
}

void Printer::OnPrintJobDone() {
  VLOG(3) << "Function: " << __FUNCTION__;
  PostOnIdle();
}

void Printer::OnLocalSettingsReceived(LocalSettings::State state,
                                      const LocalSettings& settings) {
  pending_local_settings_check_ = false;
  switch (state) {
    case LocalSettings::CURRENT:
      VLOG(0) << "No new local settings";
      PostOnIdle();
      break;
    case LocalSettings::PENDING:
      VLOG(0) << "New local settings were received";
      ApplyLocalSettings(settings);
      break;
    case LocalSettings::PRINTER_DELETED:
      LOG(WARNING) << "Printer was deleted on server";
      pending_deletion_ = true;
      PostOnIdle();
      break;

    default:
      NOTREACHED();
  }
}

void Printer::OnLocalSettingsUpdated() {
  PostOnIdle();
}

void Printer::OnXmppConnected() {
  pending_local_settings_check_ = true;
  pending_print_jobs_check_ = true;
  ChangeState(ONLINE);
  PostOnIdle();
}

void Printer::OnXmppAuthError() {
  OnAuthError();
}

void Printer::OnXmppNetworkError() {
  FallOffline(false);
}

void Printer::OnXmppNewPrintJob(const std::string& device_id) {
  DCHECK_EQ(state_.device_id, device_id) << "Data should contain printer_id";
  pending_print_jobs_check_ = true;
}

void Printer::OnXmppNewLocalSettings(const std::string& device_id) {
  DCHECK_EQ(state_.device_id, device_id) << "Data should contain printer_id";
  pending_local_settings_check_ = true;
}

void Printer::OnXmppDeleteNotification(const std::string& device_id) {
  DCHECK_EQ(state_.device_id, device_id) << "Data should contain printer_id";
  pending_deletion_ = true;
}

void Printer::TryConnect() {
  VLOG(3) << "Function: " << __FUNCTION__;

  ChangeState(CONNECTING);
  if (!requester_)
    requester_.reset(new CloudPrintRequester(GetTaskRunner(), this));

  if (IsRegistered()) {
    if (state_.access_token_update < base::Time::Now()) {
      requester_->UpdateAccesstoken(state_.refresh_token);
    } else {
      ConnectXmpp();
    }
  } else {
    // TODO(maksymb): Ping google.com to check connection state.
    ChangeState(ONLINE);
  }
}

void Printer::ConnectXmpp() {
  xmpp_listener_.reset(
      new CloudPrintXmppListener(state_.xmpp_jid,
                                 state_.local_settings.xmpp_timeout_value,
                                 GetTaskRunner(), this));
  xmpp_listener_->Connect(state_.access_token);
}

void Printer::OnIdle() {
  DCHECK(IsRegistered());
  DCHECK(on_idle_posted_) << "Instant call is not allowed";
  on_idle_posted_ = false;

  if (connection_state_ != ONLINE)
    return;

  if (pending_deletion_) {
    OnPrinterDeleted();
    return;
  }

  if (state_.access_token_update < base::Time::Now()) {
    requester_->UpdateAccesstoken(state_.refresh_token);
    return;
  }

  // TODO(maksymb): Check if privet-accesstoken was requested.

  if (pending_local_settings_check_) {
    GetLocalSettings();
    return;
  }

  if (pending_print_jobs_check_) {
    FetchPrintJobs();
    return;
  }

  base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&Printer::PostOnIdle, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(1000));
}

void Printer::FetchPrintJobs() {
  VLOG(3) << "Function: " << __FUNCTION__;
  DCHECK(IsRegistered());
  requester_->FetchPrintJobs(state_.device_id);
}

void Printer::GetLocalSettings() {
  VLOG(3) << "Function: " << __FUNCTION__;
  DCHECK(IsRegistered());
  requester_->RequestLocalSettings(state_.device_id);
}

void Printer::ApplyLocalSettings(const LocalSettings& settings) {
  state_.local_settings = settings;
  SaveToFile();

  if (state_.local_settings.local_discovery) {
    bool success = StartLocalDiscoveryServers();
    if (!success)
      LOG(ERROR) << "Local discovery servers cannot be started";
    // TODO(maksymb): If start failed try to start them again after some timeout
  } else {
    dns_server_.Shutdown();
    http_server_.Shutdown();
  }
  xmpp_listener_->set_ping_interval(state_.local_settings.xmpp_timeout_value);

  requester_->SendLocalSettings(state_.device_id, state_.local_settings);
}

void Printer::OnPrinterDeleted() {
  pending_deletion_ = false;

  state_ = PrinterState();

  SaveToFile();
  Stop();
  Start();
}

void Printer::RememberAccessToken(const std::string& access_token,
                                  int expires_in_seconds) {
  using base::Time;
  using base::TimeDelta;
  state_.access_token = access_token;
  int64 time_to_update = static_cast<int64>(expires_in_seconds *
                                            kTimeToNextAccessTokenUpdate);
  state_.access_token_update =
      Time::Now() + TimeDelta::FromSeconds(time_to_update);
  VLOG(0) << "Current access_token: " << access_token;
  SaveToFile();
}

void Printer::SetRegistrationError(const std::string& description) {
  DCHECK(!IsRegistered());
  state_.registration_state = PrinterState::REGISTRATION_ERROR;
  state_.error_description = description;
}

PrivetHttpServer::RegistrationErrorStatus Printer::CheckCommonRegErrors(
    const std::string& user) {
  CheckRegistrationExpiration();
  DCHECK(!IsRegistered());
  if (connection_state_ != ONLINE)
    return PrivetHttpServer::REG_ERROR_OFFLINE;

  if (state_.registration_state != PrinterState::UNREGISTERED &&
      user != state_.user) {
    return PrivetHttpServer::REG_ERROR_DEVICE_BUSY;
  }

  if (state_.registration_state == PrinterState::REGISTRATION_ERROR)
    return PrivetHttpServer::REG_ERROR_SERVER_ERROR;

  DCHECK_EQ(connection_state_, ONLINE);

  return PrivetHttpServer::REG_ERROR_OK;
}

void Printer::WaitUserConfirmation(base::Time valid_until) {
  // TODO(maksymb): Move to separate class.

  if (base::Time::Now() > valid_until) {
    state_.confirmation_state = PrinterState::CONFIRMATION_TIMEOUT;
    VLOG(0) << "Confirmation timeout reached.";
    return;
  }

  if (_kbhit()) {
    int c = _getche();
    if (c == 'y' || c == 'Y') {
      state_.confirmation_state = PrinterState::CONFIRMATION_CONFIRMED;
      VLOG(0) << "Registration confirmed by user.";
    } else {
      state_.confirmation_state = PrinterState::CONFIRMATION_DISCARDED;
      VLOG(0) << "Registration discarded by user.";
    }
    return;
  }

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&Printer::WaitUserConfirmation, AsWeakPtr(), valid_until),
      base::TimeDelta::FromMilliseconds(100));
}

std::string Printer::GenerateProxyId() const {
  return "{" + base::GenerateGUID() +"}";
}

std::vector<std::string> Printer::CreateTxt() const {
  std::vector<std::string> txt;
  txt.push_back("txtvers=1");
  txt.push_back("ty=" + std::string(kPrinterName));
  txt.push_back("note=" + std::string(GetDescription()));
  txt.push_back("url=" + std::string(kCloudPrintUrl));
  txt.push_back("type=printer");
  txt.push_back("id=" + state_.device_id);
  txt.push_back("cs=" + ConnectionStateToString(connection_state_));

  return txt;
}

void Printer::SaveToFile() {
  GetCapabilities();  // Make sure capabilities created.
  base::FilePath file_path;
  file_path = file_path.AppendASCII(
      command_line_reader::ReadStatePath(kPrinterStatePathDefault));

  if (printer_state::SaveToFile(file_path, state_)) {
    VLOG(0) << "Printer state written to file";
  } else {
    VLOG(0) << "Cannot write printer state to file";
  }
}

bool Printer::LoadFromFile() {
  state_ = PrinterState();

  base::FilePath file_path;
  file_path = file_path.AppendASCII(
      command_line_reader::ReadStatePath(kPrinterStatePathDefault));

  if (!base::PathExists(file_path)) {
    VLOG(0) << "Printer state file not found";
    return false;
  }

  if (printer_state::LoadFromFile(file_path, &state_)) {
    VLOG(0) << "Printer state loaded from file";
    SaveToFile();
  } else {
    VLOG(0) << "Reading/parsing printer state from file failed";
  }

  return true;
}

void Printer::PostOnIdle() {
  VLOG(3) << "Function: " << __FUNCTION__;
  DCHECK(!on_idle_posted_) << "Only one instance can be posted.";
  on_idle_posted_ = true;

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&Printer::OnIdle, AsWeakPtr()));
}

void Printer::CheckRegistrationExpiration() {
  if (!registration_expiration_.is_null() &&
      registration_expiration_ < base::Time::Now()) {
    state_ = PrinterState();
    InvalidateRegistrationExpiration();
  }
}

void Printer::UpdateRegistrationExpiration() {
  registration_expiration_ =
      base::Time::Now() + base::TimeDelta::FromSeconds(kRegistrationTimeout);
}

void Printer::InvalidateRegistrationExpiration() {
  registration_expiration_ = base::Time();
}

bool Printer::StartLocalDiscoveryServers() {
  if (!StartHttpServer())
    return false;
  if (!StartDnsServer()) {
    http_server_.Shutdown();
    return false;
  }
  return true;
}

bool Printer::StartDnsServer() {
  DCHECK(state_.local_settings.local_discovery);

  // TODO(maksymb): Add switch for command line to control interface name.
  net::IPAddressNumber ip = GetLocalIp("", false);
  if (ip.empty()) {
    LOG(ERROR) << "No local IP found. Cannot start printer.";
    return false;
  }
  VLOG(0) << "Local address: " << net::IPAddressToString(ip);

  uint16 port = command_line_reader::ReadHttpPort(kHttpPortDefault);

  std::string service_name_prefix =
      command_line_reader::ReadServiceNamePrefix(kServiceNamePrefixDefault +
                                                 net::IPAddressToString(ip));
  std::replace(service_name_prefix .begin(), service_name_prefix .end(),
               '.', '_');

  std::string service_domain_name =
      command_line_reader::ReadDomainName(
          base::StringPrintf(kServiceDomainNameFormatDefault,
                             base::RandInt(0, INT_MAX)));

  ServiceParameters params(kServiceType, kSecondaryServiceType,
                           service_name_prefix, service_domain_name,
                           ip, GetLocalIp("", true), port);

  return dns_server_.Start(params,
                           command_line_reader::ReadTtl(kTtlDefault),
                           CreateTxt());
}

bool Printer::StartHttpServer() {
  DCHECK(state_.local_settings.local_discovery);
  using command_line_reader::ReadHttpPort;
  return http_server_.Start(ReadHttpPort(kHttpPortDefault));
}

PrivetHttpServer::RegistrationErrorStatus
Printer::ConfirmationToRegistrationError(
    PrinterState::ConfirmationState state) {
  switch (state) {
    case PrinterState::CONFIRMATION_PENDING:
      return PrivetHttpServer::REG_ERROR_PENDING_USER_ACTION;
    case PrinterState::CONFIRMATION_DISCARDED:
      return PrivetHttpServer::REG_ERROR_USER_CANCEL;
    case PrinterState::CONFIRMATION_CONFIRMED:
      NOTREACHED();
      return PrivetHttpServer::REG_ERROR_OK;
    case PrinterState::CONFIRMATION_TIMEOUT:
      return PrivetHttpServer::REG_ERROR_CONFIRMATION_TIMEOUT;
    default:
      NOTREACHED();
      return PrivetHttpServer::REG_ERROR_OK;
  }
}

std::string Printer::ConnectionStateToString(ConnectionState state) const {
  switch (state) {
    case OFFLINE:
      return "offline";
    case ONLINE:
      return "online";
    case CONNECTING:
      return "connecting";
    case NOT_CONFIGURED:
      return "not-configured";

    default:
      NOTREACHED();
      return "";
  }
}

void Printer::FallOffline(bool instant_reconnect) {
  bool changed = ChangeState(OFFLINE);
  DCHECK(changed) << "Falling offline from offline is now allowed";

  if (!IsRegistered())
    SetRegistrationError("Cannot access server during registration process");

  if (instant_reconnect) {
    TryConnect();
  } else {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&Printer::TryConnect, AsWeakPtr()),
        base::TimeDelta::FromSeconds(kReconnectTimeout));
  }
}

bool Printer::ChangeState(ConnectionState new_state) {
  if (connection_state_ == new_state)
    return false;

  connection_state_ = new_state;
  VLOG(0) << base::StringPrintf(
      "Printer is now %s (%s)",
      ConnectionStateToString(connection_state_).c_str(),
      IsRegistered() ? "registered" : "unregistered");

  dns_server_.UpdateMetadata(CreateTxt());

  if (connection_state_ == OFFLINE) {
    requester_.reset();
    xmpp_listener_.reset();
  }

  return true;
}

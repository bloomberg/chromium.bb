// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CookieAccountant implementation.
#include "ceee/ie/plugin/bho/cookie_accountant.h"

#include <atlbase.h>
#include <wininet.h>

#include <set>

#include "base/format_macros.h"  // For PRIu64.
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_tokenizer.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/broker/cookie_api_module.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/common/ie_util.h"

namespace {

static const char kSetCookieHeaderName[] = "Set-Cookie:";
static const char kHttpResponseHeaderDelimiter[] = "\n";
static const wchar_t kMsHtmlModuleName[] = L"mshtml.dll";
static const char kWinInetModuleName[] = "wininet.dll";
static const char kInternetSetCookieExAFunctionName[] = "InternetSetCookieExA";
static const char kInternetSetCookieExWFunctionName[] = "InternetSetCookieExW";

}  // namespace

CookieAccountant* CookieAccountant::singleton_instance_ = NULL;

CookieAccountant::~CookieAccountant() {
  if (internet_set_cookie_ex_a_patch_.is_patched())
    internet_set_cookie_ex_a_patch_.Unpatch();
  if (internet_set_cookie_ex_w_patch_.is_patched())
    internet_set_cookie_ex_w_patch_.Unpatch();
}

ProductionCookieAccountant::ProductionCookieAccountant() {
}

// static
ProductionCookieAccountant* ProductionCookieAccountant::GetInstance() {
  return Singleton<ProductionCookieAccountant>::get();
}

CookieAccountant* CookieAccountant::GetInstance() {
  // Unit tests can set singleton_instance_ directly.
  if (singleton_instance_ == NULL)
    singleton_instance_ = ProductionCookieAccountant::GetInstance();
  return singleton_instance_;
}

DWORD WINAPI CookieAccountant::InternetSetCookieExAPatch(
    LPCSTR url, LPCSTR cookie_name, LPCSTR cookie_data,
    DWORD flags, DWORD_PTR reserved) {
  base::Time current_time = base::Time::Now();
  DWORD cookie_state = ::InternetSetCookieExA(url, cookie_name, cookie_data,
                                              flags, reserved);
  CookieAccountant::GetInstance()->RecordCookie(url, cookie_data,
                                                current_time);
  return cookie_state;
}

DWORD WINAPI CookieAccountant::InternetSetCookieExWPatch(
    LPCWSTR url, LPCWSTR cookie_name, LPCWSTR cookie_data,
    DWORD flags, DWORD_PTR reserved) {
  base::Time current_time = base::Time::Now();
  DWORD cookie_state = ::InternetSetCookieExW(url, cookie_name, cookie_data,
                                              flags, reserved);
  CookieAccountant::GetInstance()->RecordCookie(
      std::string(CW2A(url)), std::string(CW2A(cookie_data)), current_time);
  return cookie_state;
}

class CurrentProcessFilter : public base::ProcessFilter {
 public:
  CurrentProcessFilter() : current_process_id_(base::GetCurrentProcId()) {
  }

  virtual bool Includes(const base::ProcessEntry& entry) const {
    return entry.pid() == current_process_id_;
  }

 private:
  base::ProcessId current_process_id_;

  DISALLOW_COPY_AND_ASSIGN(CurrentProcessFilter);
};

// TODO(cindylau@chromium.org): Make this function more robust.
void CookieAccountant::RecordCookie(
    const std::string& url, const std::string& cookie_data,
    const base::Time& current_time) {
  cookie_api::CookieInfo cookie;
  std::string cookie_data_string = cookie_data;
  net::CookieMonster::ParsedCookie parsed_cookie(cookie_data_string);
  DCHECK(parsed_cookie.IsValid());
  if (!parsed_cookie.IsValid())
    return;

  // Fill the cookie info from the parsed cookie.
  // TODO(cindylau@chromium.org): Add a helper function to convert an
  // std::string to a BSTR.
  cookie.name = ::SysAllocString(ASCIIToWide(parsed_cookie.Name()).c_str());
  cookie.value = ::SysAllocString(ASCIIToWide(parsed_cookie.Value()).c_str());
  SetScriptCookieDomain(parsed_cookie, &cookie);
  SetScriptCookiePath(parsed_cookie, &cookie);
  cookie.secure = parsed_cookie.IsSecure() ? TRUE : FALSE;
  cookie.http_only = parsed_cookie.IsHttpOnly() ? TRUE : FALSE;
  SetScriptCookieExpirationDate(parsed_cookie, current_time, &cookie);
  SetScriptCookieStoreId(&cookie);

  // Send the cookie event to the broker.
  // TODO(cindylau@chromium.org): Set the removed parameter properly.
  cookie_events_funnel().OnChanged(false, cookie);
}

void CookieAccountant::SetScriptCookieDomain(
    const net::CookieMonster::ParsedCookie& parsed_cookie,
    cookie_api::CookieInfo* cookie) {
  if (parsed_cookie.HasDomain()) {
    cookie->domain = ::SysAllocString(
        ASCIIToWide(parsed_cookie.Domain()).c_str());
    cookie->host_only = FALSE;
  } else {
    // TODO(cindylau@chromium.org): If the domain is not provided, get
    // it from the URL.
    cookie->host_only = TRUE;
  }
}

void CookieAccountant::SetScriptCookiePath(
    const net::CookieMonster::ParsedCookie& parsed_cookie,
    cookie_api::CookieInfo* cookie) {
  // TODO(cindylau@chromium.org): If the path is not provided, get it
  // from the URL.
  if (parsed_cookie.HasPath())
    cookie->path = ::SysAllocString(ASCIIToWide(parsed_cookie.Path()).c_str());
}

void CookieAccountant::SetScriptCookieExpirationDate(
    const net::CookieMonster::ParsedCookie& parsed_cookie,
    const base::Time& current_time,
    cookie_api::CookieInfo* cookie) {
  // First, try the Max-Age attribute.
  uint64 max_age = 0;
  if (parsed_cookie.HasMaxAge() &&
      sscanf_s(parsed_cookie.MaxAge().c_str(), " %" PRIu64, &max_age) == 1) {
    cookie->session = FALSE;
    base::Time expiration_time = current_time +
        base::TimeDelta::FromSeconds(max_age);
    cookie->expiration_date = expiration_time.ToDoubleT();
  } else if (parsed_cookie.HasExpires()) {
    cookie->session = FALSE;
    base::Time expiration_time = net::CookieMonster::ParseCookieTime(
        parsed_cookie.Expires());
    cookie->expiration_date = expiration_time.ToDoubleT();
  } else {
    cookie->session = TRUE;
  }
}

void CookieAccountant::SetScriptCookieStoreId(cookie_api::CookieInfo* cookie) {
  // The store ID is either the current process ID, or the process ID of the
  // parent process, if that parent process is an IE frame process.
  // First collect all IE process IDs.
  std::set<base::ProcessId> ie_pids;
  base::NamedProcessIterator ie_iter(
      ceee_module_util::kInternetExplorerModuleName, NULL);
  while (const base::ProcessEntry* process_entry = ie_iter.NextProcessEntry()) {
    ie_pids.insert(process_entry->pid());
  }
  // Now get the store ID process by finding the current process, and seeing if
  // its parent process is an IE process.
  DWORD process_id = 0;
  CurrentProcessFilter filter;
  base::ProcessIterator it(&filter);
  while (const base::ProcessEntry* process_entry = it.NextProcessEntry()) {
    // There should only be one matching process entry.
    DCHECK_EQ(process_id, DWORD(0));
    if (ie_pids.find(process_entry->parent_pid()) != ie_pids.end()) {
      process_id = process_entry->parent_pid();
    } else {
      DCHECK(ie_pids.find(process_entry->pid()) != ie_pids.end());
      process_id = process_entry->pid();
    }
  }
  DCHECK_NE(process_id, DWORD(0));
  std::ostringstream store_id_stream;
  store_id_stream << process_id;
  // If this is a Protected Mode process, the cookie store ID is different.
  bool is_protected_mode = false;
  HRESULT hr = ie_util::GetIEIsInProtectedMode(&is_protected_mode);
  // E_NOTIMPL might occur when IE protected mode is disabled, or for IE6 or
  // Windows XP. Those are not errors, so we shouldn't assert here.
  DCHECK(SUCCEEDED(hr) || hr == E_NOTIMPL) << "Unexpected failure while " <<
      "checking the protected mode setting for IE tab process " << process_id <<
      ". " << com::LogHr(hr);
  if (SUCCEEDED(hr) && is_protected_mode) {
    store_id_stream << cookie_api::kProtectedModeStoreIdSuffix;
  }
  // The broker is responsible for checking that the store ID is registered.
  cookie->store_id =
      ::SysAllocString(ASCIIToWide(store_id_stream.str()).c_str());
}

void CookieAccountant::RecordHttpResponseCookies(
    const std::string& response_headers, const base::Time& current_time) {
  StringTokenizer t(response_headers, kHttpResponseHeaderDelimiter);
  while (t.GetNext()) {
    std::string header_line = t.token();
    size_t name_pos = header_line.find(kSetCookieHeaderName);
    if (name_pos == std::string::npos)
      continue;  // Skip non-cookie headers.
    std::string cookie_data = header_line.substr(
        name_pos + std::string(kSetCookieHeaderName).size());
    // TODO(cindylau@chromium.org): Get the URL for the HTTP request from
    // IHttpNegotiate::BeginningTransaction.
    RecordCookie(std::string(), cookie_data, current_time);
  }
}

void CookieAccountant::ConnectBroker() {
  if (!broker_rpc_client_.is_connected()) {
    HRESULT hr = broker_rpc_client_.Connect(true);
    DCHECK(SUCCEEDED(hr));
  }
}

void CookieAccountant::Initialize() {
  {
    base::AutoLock lock(lock_);
    if (initializing_)
      return;
    initializing_ = true;
  }
  ConnectBroker();

  if (!internet_set_cookie_ex_a_patch_.is_patched()) {
    DWORD error = internet_set_cookie_ex_a_patch_.Patch(
        kMsHtmlModuleName, kWinInetModuleName,
        kInternetSetCookieExAFunctionName, InternetSetCookieExAPatch);
    DCHECK(error == NO_ERROR || !internet_set_cookie_ex_a_patch_.is_patched());
  }
  if (!internet_set_cookie_ex_w_patch_.is_patched()) {
    DWORD error = internet_set_cookie_ex_w_patch_.Patch(
        kMsHtmlModuleName, kWinInetModuleName,
        kInternetSetCookieExWFunctionName, InternetSetCookieExWPatch);
    DCHECK(error == NO_ERROR || !internet_set_cookie_ex_w_patch_.is_patched());
  }
  DCHECK(internet_set_cookie_ex_a_patch_.is_patched() ||
         internet_set_cookie_ex_w_patch_.is_patched());
  {
    base::AutoLock lock(lock_);
    initializing_ = false;
  }
}

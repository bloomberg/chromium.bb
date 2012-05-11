// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/base_session_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/referrer.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebReferrerPolicy;
using content::BrowserThread;
using content::NavigationEntry;

// InternalGetCommandsRequest -------------------------------------------------

BaseSessionService::InternalGetCommandsRequest::~InternalGetCommandsRequest() {
  STLDeleteElements(&commands);
}

// BaseSessionService ---------------------------------------------------------

namespace {

// Helper used by CreateUpdateTabNavigationCommand(). It writes |str| to
// |pickle|, if and only if |str| fits within (|max_bytes| - |*bytes_written|).
// |bytes_written| is incremented to reflect the data written.
void WriteStringToPickle(Pickle& pickle, int* bytes_written, int max_bytes,
                         const std::string& str) {
  int num_bytes = str.size() * sizeof(char);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle.WriteString(str);
  } else {
    pickle.WriteString(std::string());
  }
}

// string16 version of WriteStringToPickle.
void WriteString16ToPickle(Pickle& pickle, int* bytes_written, int max_bytes,
                           const string16& str) {
  int num_bytes = str.size() * sizeof(char16);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle.WriteString16(str);
  } else {
    pickle.WriteString16(string16());
  }
}

}  // namespace

// Delay between when a command is received, and when we save it to the
// backend.
static const int kSaveDelayMS = 2500;

// static
const int BaseSessionService::max_persist_navigation_count = 6;

BaseSessionService::BaseSessionService(SessionType type,
                                       Profile* profile,
                                       const FilePath& path)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      pending_reset_(false),
      commands_since_reset_(0),
      save_post_data_(false) {
  if (profile) {
    // We should never be created when incognito.
    DCHECK(!profile->IsOffTheRecord());
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    save_post_data_ =
        !command_line->HasSwitch(switches::kDisableRestoreSessionState);
  }
  backend_ = new SessionBackend(type, profile_ ? profile_->GetPath() : path);
  DCHECK(backend_.get());

  RunTaskOnBackendThread(FROM_HERE,
                         base::Bind(&SessionBackend::Init, backend_));
}

BaseSessionService::~BaseSessionService() {
}

void BaseSessionService::DeleteLastSession() {
  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::DeleteLastSession, backend()));
}

void BaseSessionService::ScheduleCommand(SessionCommand* command) {
  DCHECK(command);
  commands_since_reset_++;
  pending_commands_.push_back(command);
  StartSaveTimer();
}

void BaseSessionService::StartSaveTimer() {
  // Don't start a timer when testing (profile == NULL or
  // MessageLoop::current() is NULL).
  if (MessageLoop::current() && profile() && !weak_factory_.HasWeakPtrs()) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BaseSessionService::Save, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kSaveDelayMS));
  }
}

void BaseSessionService::Save() {
  DCHECK(backend());

  if (pending_commands_.empty())
    return;

  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::AppendCommands, backend(),
                 new std::vector<SessionCommand*>(pending_commands_),
                 pending_reset_));

  // Backend took ownership of commands.
  pending_commands_.clear();

  if (pending_reset_) {
    commands_since_reset_ = 0;
    pending_reset_ = false;
  }
}

SessionCommand* BaseSessionService::CreateUpdateTabNavigationCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    int index,
    const NavigationEntry& entry) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(tab_id);
  pickle.WriteInt(index);

  // We only allow navigations up to 63k (which should be completely
  // reasonable). On the off chance we get one that is too big, try to
  // keep the url.

  // Bound the string data (which is variable length) to
  // |max_state_size bytes| bytes.
  static const SessionCommand::size_type max_state_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_state_size,
                      entry.GetVirtualURL().spec());

  WriteString16ToPickle(pickle, &bytes_written, max_state_size,
                        entry.GetTitle());

  std::string content_state = entry.GetContentState();
  if (entry.GetHasPostData()) {
    if (save_post_data_) {
      content_state =
          webkit_glue::RemovePasswordDataFromHistoryState(content_state);
    } else {
      content_state =
          webkit_glue::RemoveFormDataFromHistoryState(content_state);
    }
  }
  WriteStringToPickle(pickle, &bytes_written, max_state_size, content_state);

  pickle.WriteInt(entry.GetTransitionType());
  int type_mask = entry.GetHasPostData() ? TabNavigation::HAS_POST_DATA : 0;
  pickle.WriteInt(type_mask);

  WriteStringToPickle(pickle, &bytes_written, max_state_size,
      entry.GetReferrer().url.is_valid() ?
          entry.GetReferrer().url.spec() : std::string());
  pickle.WriteInt(entry.GetReferrer().policy);

  // Save info required to override the user agent.
  WriteStringToPickle(pickle, &bytes_written, max_state_size,
      entry.GetOriginalRequestURL().is_valid() ?
          entry.GetOriginalRequestURL().spec() : std::string());
  pickle.WriteBool(entry.GetIsOverridingUserAgent());

  return new SessionCommand(command_id, pickle);
}

SessionCommand* BaseSessionService::CreateSetTabExtensionAppIDCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const std::string& extension_id) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(tab_id);

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_id_size, extension_id);

  return new SessionCommand(command_id, pickle);
}

SessionCommand* BaseSessionService::CreateSetTabUserAgentOverrideCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const std::string& user_agent_override) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(tab_id);

  // Enforce a max for the user agent length.  They should never be anywhere
  // near this size.
  static const SessionCommand::size_type max_user_agent_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_user_agent_size,
      user_agent_override);

  return new SessionCommand(command_id, pickle);
}

SessionCommand* BaseSessionService::CreateSetWindowAppNameCommand(
    SessionID::id_type command_id,
    SessionID::id_type window_id,
    const std::string& app_name) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(window_id);

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_id_size, app_name);

  return new SessionCommand(command_id, pickle);
}

bool BaseSessionService::RestoreUpdateTabNavigationCommand(
    const SessionCommand& command,
    TabNavigation* navigation,
    SessionID::id_type* tab_id) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;
  PickleIterator iterator(*pickle);
  std::string url_spec;
  if (!pickle->ReadInt(&iterator, tab_id) ||
      !pickle->ReadInt(&iterator, &(navigation->index_)) ||
      !pickle->ReadString(&iterator, &url_spec) ||
      !pickle->ReadString16(&iterator, &(navigation->title_)) ||
      !pickle->ReadString(&iterator, &(navigation->state_)) ||
      !pickle->ReadInt(&iterator,
                       reinterpret_cast<int*>(&(navigation->transition_))))
    return false;
  // type_mask did not always exist in the written stream. As such, we
  // don't fail if it can't be read.
  bool has_type_mask = pickle->ReadInt(&iterator, &(navigation->type_mask_));

  if (has_type_mask) {
    // the "referrer" property was added after type_mask to the written
    // stream. As such, we don't fail if it can't be read.
    std::string referrer_spec;
    pickle->ReadString(&iterator, &referrer_spec);
    // The "referrer policy" property was added even later, so we fall back to
    // the default policy if the property is not present.
    int policy_int;
    WebReferrerPolicy policy;
    if (pickle->ReadInt(&iterator, &policy_int))
      policy = static_cast<WebReferrerPolicy>(policy_int);
    else
      policy = WebKit::WebReferrerPolicyDefault;
    navigation->referrer_ = content::Referrer(
        referrer_spec.empty() ? GURL() : GURL(referrer_spec),
        policy);

    // If the original URL can't be found, leave it empty.
    std::string url_spec;
    if (!pickle->ReadString(&iterator, &url_spec))
      url_spec = std::string();
    navigation->set_original_request_url(GURL(url_spec));

    // Default to not overriding the user agent if we don't have info.
    bool override_user_agent;
    if (!pickle->ReadBool(&iterator, &override_user_agent))
      override_user_agent = false;
    navigation->set_is_overriding_user_agent(override_user_agent);
  }

  navigation->virtual_url_ = GURL(url_spec);
  return true;
}

bool BaseSessionService::RestoreSetTabExtensionAppIDCommand(
    const SessionCommand& command,
    SessionID::id_type* tab_id,
    std::string* extension_app_id) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;

  PickleIterator iterator(*pickle);
  return pickle->ReadInt(&iterator, tab_id) &&
      pickle->ReadString(&iterator, extension_app_id);
}

bool BaseSessionService::RestoreSetTabUserAgentOverrideCommand(
    const SessionCommand& command,
    SessionID::id_type* tab_id,
    std::string* user_agent_override) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;

  PickleIterator iterator(*pickle);
  return pickle->ReadInt(&iterator, tab_id) &&
      pickle->ReadString(&iterator, user_agent_override);
}

bool BaseSessionService::RestoreSetWindowAppNameCommand(
    const SessionCommand& command,
    SessionID::id_type* window_id,
    std::string* app_name) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;

  PickleIterator iterator(*pickle);
  return pickle->ReadInt(&iterator, window_id) &&
      pickle->ReadString(&iterator, app_name);
}

bool BaseSessionService::ShouldTrackEntry(const GURL& url) {
  // NOTE: Do not track print preview tab because re-opening that page will
  // just display a non-functional print preview page.
  return url.is_valid() && url != GURL(chrome::kChromeUIPrintURL);
}

BaseSessionService::Handle BaseSessionService::ScheduleGetLastSessionCommands(
    InternalGetCommandsRequest* request,
    CancelableRequestConsumerBase* consumer) {
  scoped_refptr<InternalGetCommandsRequest> request_wrapper(request);
  AddRequest(request, consumer);
  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::ReadLastSessionCommands, backend(),
                 request_wrapper));
  return request->handle();
}

bool BaseSessionService::RunTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  if (profile_ && BrowserThread::IsMessageLoopValid(BrowserThread::FILE)) {
    return BrowserThread::PostTask(BrowserThread::FILE, from_here, task);
  } else {
    // Fall back to executing on the main thread if the file thread
    // has gone away (around shutdown time) or if we're running as
    // part of a unit test that does not set profile_.
    task.Run();
    return true;
  }
}

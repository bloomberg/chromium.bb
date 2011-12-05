// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/base_session_service.h"

#include "base/bind.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebReferrerPolicy.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebReferrerPolicy;

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
      path_(path),
      backend_thread_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      pending_reset_(false),
      commands_since_reset_(0) {
  if (profile) {
    // We should never be created when incognito.
    DCHECK(!profile->IsOffTheRecord());
  }
  backend_ = new SessionBackend(type,
      profile_ ? profile_->GetPath() : path_);
  DCHECK(backend_.get());
  backend_thread_ = g_browser_process->file_thread();
  if (!backend_thread_)
    backend_->Init();
  // If backend_thread is non-null, backend will init itself as appropriate.
}

BaseSessionService::~BaseSessionService() {
}

void BaseSessionService::DeleteLastSession() {
  if (!backend_thread()) {
    backend()->DeleteLastSession();
  } else {
    backend_thread()->message_loop()->PostTask(
        FROM_HERE, base::Bind(&SessionBackend::DeleteLastSession, backend()));
  }
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
        kSaveDelayMS);
  }
}

void BaseSessionService::Save() {
  DCHECK(backend());

  if (pending_commands_.empty())
    return;

  if (!backend_thread()) {
    backend()->AppendCommands(
        new std::vector<SessionCommand*>(pending_commands_), pending_reset_);
  } else {
    backend_thread()->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&SessionBackend::AppendCommands, backend(),
                   new std::vector<SessionCommand*>(pending_commands_),
                   pending_reset_));
  }
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
                      entry.virtual_url().spec());

  WriteString16ToPickle(pickle, &bytes_written, max_state_size, entry.title());

  if (entry.has_post_data()) {
    // Remove the form data, it may contain sensitive information.
    WriteStringToPickle(pickle, &bytes_written, max_state_size,
        webkit_glue::RemoveFormDataFromHistoryState(entry.content_state()));
  } else {
    WriteStringToPickle(pickle, &bytes_written, max_state_size,
                        entry.content_state());
  }

  pickle.WriteInt(entry.transition_type());
  int type_mask = entry.has_post_data() ? TabNavigation::HAS_POST_DATA : 0;
  pickle.WriteInt(type_mask);

  WriteStringToPickle(pickle, &bytes_written, max_state_size,
      entry.referrer().url.is_valid() ?
          entry.referrer().url.spec() : std::string());
  pickle.WriteInt(entry.referrer().policy);

  // Adding more data? Be sure and update TabRestoreService too.
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

bool BaseSessionService::RestoreUpdateTabNavigationCommand(
    const SessionCommand& command,
    TabNavigation* navigation,
    SessionID::id_type* tab_id) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;
  void* iterator = NULL;
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

  void* iterator = NULL;
  return pickle->ReadInt(&iterator, tab_id) &&
      pickle->ReadString(&iterator, extension_app_id);
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
  if (backend_thread()) {
    backend_thread()->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&SessionBackend::ReadLastSessionCommands, backend(),
                   request_wrapper));
  } else {
    backend()->ReadLastSessionCommands(request);
  }
  return request->handle();
}

BaseSessionService::Handle
    BaseSessionService::ScheduleGetCurrentSessionCommands(
        InternalGetCommandsRequest* request,
        CancelableRequestConsumerBase* consumer) {
  scoped_refptr<InternalGetCommandsRequest> request_wrapper(request);
  AddRequest(request, consumer);
  if (backend_thread()) {
    backend_thread()->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&SessionBackend::ReadCurrentSessionCommands, backend(),
                   request_wrapper));
  } else {
    backend()->ReadCurrentSessionCommands(request);
  }
  return request->handle();
}

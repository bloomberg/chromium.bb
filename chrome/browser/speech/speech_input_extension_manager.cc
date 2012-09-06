// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_extension_manager.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::SpeechRecognitionHypothesis;
using content::SpeechRecognitionManager;

namespace {

const char kErrorNoRecordingDeviceFound[] = "noRecordingDeviceFound";
const char kErrorRecordingDeviceInUse[] = "recordingDeviceInUse";
const char kErrorUnableToStart[] = "unableToStart";
const char kErrorRequestDenied[] = "requestDenied";
const char kErrorRequestInProgress[] = "requestInProgress";
const char kErrorInvalidOperation[] = "invalidOperation";

const char kErrorCodeKey[] = "code";
const char kErrorCaptureError[] = "captureError";
const char kErrorNetworkError[] = "networkError";
const char kErrorNoSpeechHeard[] = "noSpeechHeard";
const char kErrorNoResults[] = "noResults";

const char kUtteranceKey[] = "utterance";
const char kConfidenceKey[] = "confidence";
const char kHypothesesKey[] = "hypotheses";

const char kOnErrorEvent[] = "experimental.speechInput.onError";
const char kOnResultEvent[] = "experimental.speechInput.onResult";
const char kOnSoundStartEvent[] = "experimental.speechInput.onSoundStart";
const char kOnSoundEndEvent[] = "experimental.speechInput.onSoundEnd";

// Wrap an SpeechInputExtensionManager using scoped_refptr to avoid
// assertion failures on destruction because of not using release().
class SpeechInputExtensionManagerWrapper : public ProfileKeyedService {
 public:
  explicit SpeechInputExtensionManagerWrapper(
      SpeechInputExtensionManager* manager)
      : manager_(manager) {}

  virtual ~SpeechInputExtensionManagerWrapper() {}

  SpeechInputExtensionManager* manager() const { return manager_.get(); }

 private:
  // Methods from ProfileKeyedService.
  virtual void Shutdown() OVERRIDE {
    manager()->ShutdownOnUIThread();
  }

  scoped_refptr<SpeechInputExtensionManager> manager_;
};
}

// Factory for SpeechInputExtensionManagers as profile keyed services.
class SpeechInputExtensionManager::Factory : public ProfileKeyedServiceFactory {
 public:
  static void Initialize();
  static Factory* GetInstance();

  SpeechInputExtensionManagerWrapper* GetForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<Factory>;

  Factory();
  virtual ~Factory();

  // ProfileKeyedServiceFactory methods:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE { return false; }
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE { return true; }
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE { return true; }

  DISALLOW_COPY_AND_ASSIGN(Factory);
};

void SpeechInputExtensionManager::Factory::Initialize() {
  GetInstance();
}

SpeechInputExtensionManager::Factory*
    SpeechInputExtensionManager::Factory::GetInstance() {
  return Singleton<SpeechInputExtensionManager::Factory>::get();
}

SpeechInputExtensionManagerWrapper*
    SpeechInputExtensionManager::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<SpeechInputExtensionManagerWrapper*>(
      GetServiceForProfile(profile, true));
}

SpeechInputExtensionManager::Factory::Factory()
    : ProfileKeyedServiceFactory("SpeechInputExtensionManager",
                                 ProfileDependencyManager::GetInstance()) {
}

SpeechInputExtensionManager::Factory::~Factory() {
}

ProfileKeyedService*
    SpeechInputExtensionManager::Factory::BuildServiceInstanceFor(
    Profile* profile) const {
  scoped_refptr<SpeechInputExtensionManager> manager(
      new SpeechInputExtensionManager(profile));
  return new SpeechInputExtensionManagerWrapper(manager);
}

SpeechInputExtensionInterface::SpeechInputExtensionInterface() {
}

SpeechInputExtensionInterface::~SpeechInputExtensionInterface() {
}

SpeechInputExtensionManager::SpeechInputExtensionManager(Profile* profile)
    : profile_(profile),
      state_(kIdle),
      registrar_(new content::NotificationRegistrar),
      speech_interface_(NULL),
      is_recognition_in_progress_(false),
      speech_recognition_session_id_(
          SpeechRecognitionManager::kSessionIDInvalid) {
  registrar_->Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                  content::Source<Profile>(profile_));
}

SpeechInputExtensionManager::~SpeechInputExtensionManager() {
}

SpeechInputExtensionManager* SpeechInputExtensionManager::GetForProfile(
    Profile* profile) {
  SpeechInputExtensionManagerWrapper* wrapper =
      Factory::GetInstance()->GetForProfile(profile);
  if (!wrapper)
    return NULL;
  return wrapper->manager();
}

void SpeechInputExtensionManager::InitializeFactory() {
  Factory::Initialize();
}

void SpeechInputExtensionManager::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    ExtensionUnloaded(
        content::Details<extensions::UnloadedExtensionInfo>(details)->
            extension->id());
  } else {
    NOTREACHED();
  }
}

void SpeechInputExtensionManager::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Profile shutting down.";

  // Note: Unretained(this) is safe, also if we are freed in the meanwhile.
  // It is used by the SR manager just for comparing the raw pointer and remove
  // the associated sessions.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::AbortAllSessionsOnIOThread,
                 base::Unretained(this)));

  base::AutoLock auto_lock(state_lock_);
  DCHECK(state_ != kShutdown);
  if (state_ != kIdle) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&SpeechInputExtensionManager::ForceStopOnIOThread, this));
  }
  state_ = kShutdown;
  VLOG(1) << "Entering the shutdown sink state.";
  registrar_.reset();
  profile_ = NULL;
}

void SpeechInputExtensionManager::AbortAllSessionsOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(primiano): The following check should not be really needed if the
  // SpeechRecognitionManager and this class are destroyed in the correct order
  // (this class first), as it is in current chrome implementation.
  // However, it seems the some ChromiumOS tests violate the destruction order
  // envisaged by browser_main_loop, so SpeechRecognitionmanager could have been
  // freed by now.
  if (SpeechRecognitionManager* mgr = SpeechRecognitionManager::GetInstance())
    mgr->AbortAllSessionsForListener(this);
}

void SpeechInputExtensionManager::ExtensionUnloaded(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  VLOG(1) << "Extension unloaded. Requesting to enforce stop...";
  if (extension_id_in_use_ == extension_id) {
    if (state_ != kIdle) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
          base::Bind(&SpeechInputExtensionManager::ForceStopOnIOThread, this));
    }
  }
}

void SpeechInputExtensionManager::SetSpeechInputExtensionInterface(
    SpeechInputExtensionInterface* speech_interface) {
  speech_interface_ = speech_interface;
}

SpeechInputExtensionInterface*
    SpeechInputExtensionManager::GetSpeechInputExtensionInterface() {
  return speech_interface_ ? speech_interface_ : this;
}

void SpeechInputExtensionManager::ResetToIdleState() {
  VLOG(1) << "State changed to idle. Deassociating any extensions.";
  state_ = kIdle;
  extension_id_in_use_.clear();
}

int SpeechInputExtensionManager::GetRenderProcessIDForExtension(
    const std::string& extension_id) const {
  ExtensionProcessManager* epm =
      extensions::ExtensionSystem::Get(profile_)->process_manager();
  DCHECK(epm);
  extensions::ExtensionHost* eh =
      epm->GetBackgroundHostForExtension(extension_id);
  DCHECK(eh);
  content::RenderProcessHost* rph = eh->render_process_host();
  DCHECK(rph);
  return rph->GetID();
}

void SpeechInputExtensionManager::OnRecognitionResult(
    int session_id,
    const content::SpeechRecognitionResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, speech_recognition_session_id_);

  // Stopping will start the disassociation with the extension.
  // Make a copy to report the results to the proper one.
  std::string extension_id = extension_id_in_use_;
  ForceStopOnIOThread();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::SetRecognitionResultOnUIThread,
      this, result, extension_id));
}

void SpeechInputExtensionManager::SetRecognitionResultOnUIThread(
    const content::SpeechRecognitionResult& result,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* js_event = new DictionaryValue();
  args->Append(js_event);

  ListValue* js_hypothesis_array = new ListValue();
  js_event->Set(kHypothesesKey, js_hypothesis_array);

  for (size_t i = 0; i < result.hypotheses.size(); ++i) {
    const SpeechRecognitionHypothesis& hypothesis = result.hypotheses[i];

    DictionaryValue* js_hypothesis_object = new DictionaryValue();
    js_hypothesis_array->Append(js_hypothesis_object);

    js_hypothesis_object->SetString(kUtteranceKey,
        UTF16ToUTF8(hypothesis.utterance));
    js_hypothesis_object->SetDouble(kConfidenceKey,
        hypothesis.confidence);
  }

  DispatchEventToExtension(extension_id, kOnResultEvent, args.Pass());
}

void SpeechInputExtensionManager::OnRecognitionStart(int session_id) {
  DCHECK_EQ(session_id, speech_recognition_session_id_);
}

void SpeechInputExtensionManager::OnAudioStart(int session_id) {
  VLOG(1) << "OnAudioStart";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, speech_recognition_session_id_);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::DidStartReceivingAudioOnUIThread,
      this));
}

void SpeechInputExtensionManager::OnAudioEnd(int session_id) {
}

void SpeechInputExtensionManager::OnRecognitionEnd(int session_id) {
  // In the very exceptional case in which we requested a new recognition before
  // the previous one ended, don't clobber the speech_recognition_session_id_.
  if (speech_recognition_session_id_ == session_id) {
    is_recognition_in_progress_ = false;
    speech_recognition_session_id_ =
        SpeechRecognitionManager::kSessionIDInvalid;
  }
}

void SpeechInputExtensionManager::DidStartReceivingAudioOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  DCHECK_EQ(state_, kStarting);
  VLOG(1) << "State changed to recording";
  state_ = kRecording;

  DCHECK(profile_);

  VLOG(1) << "Sending start notification";
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STARTED,
      content::Source<Profile>(profile_),
      content::Details<std::string>(&extension_id_in_use_));
}

void SpeechInputExtensionManager::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, speech_recognition_session_id_);
  VLOG(1) << "OnRecognitionError: " << error.code;

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  GetSpeechInputExtensionInterface()->StopRecording(true);

  std::string event_error_code;
  bool report_to_event = true;

  switch (error.code) {
    case content::SPEECH_RECOGNITION_ERROR_NONE:
      break;

    case content::SPEECH_RECOGNITION_ERROR_ABORTED:
      // ERROR_ABORTED is received whenever AbortSession is called on the
      // manager. However, we want propagate the error only if it is triggered
      // by an external cause (another recognition started, aborting us), thus
      // only if it occurs while we are capturing audio.
      if (state_ == kRecording)
        event_error_code = kErrorCaptureError;
      break;

    case content::SPEECH_RECOGNITION_ERROR_AUDIO:
      if (state_ == kStarting) {
        event_error_code = kErrorUnableToStart;
        report_to_event = false;
      } else {
        event_error_code = kErrorCaptureError;
      }
      break;

    case content::SPEECH_RECOGNITION_ERROR_NETWORK:
      event_error_code = kErrorNetworkError;
      break;

    case content::SPEECH_RECOGNITION_ERROR_BAD_GRAMMAR:
      // No error is returned on invalid language, for example.
      // To avoid confusion about when this is would be fired, the invalid
      // params error is not being exposed to the onError event.
      event_error_code = kErrorUnableToStart;
      break;

    case content::SPEECH_RECOGNITION_ERROR_NO_SPEECH:
      event_error_code = kErrorNoSpeechHeard;
      break;

    case content::SPEECH_RECOGNITION_ERROR_NO_MATCH:
      event_error_code = kErrorNoResults;
      break;

    // The remaining kErrorAborted case should never be returned by the server.
    default:
      NOTREACHED();
  }

  if (!event_error_code.empty()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechInputExtensionManager::DispatchError,
        this, event_error_code, report_to_event));
  }
}

void SpeechInputExtensionManager::OnEnvironmentEstimationComplete(
    int session_id) {
  DCHECK_EQ(session_id, speech_recognition_session_id_);
}

void SpeechInputExtensionManager::OnSoundStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, speech_recognition_session_id_);
  VLOG(1) << "OnSoundStart";

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::DispatchEventToExtension,
      this, extension_id_in_use_, std::string(kOnSoundStartEvent),
      Passed(scoped_ptr<ListValue>(new ListValue()))));
}

void SpeechInputExtensionManager::OnSoundEnd(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "OnSoundEnd";

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::DispatchEventToExtension,
      this, extension_id_in_use_, std::string(kOnSoundEndEvent),
      Passed(scoped_ptr<ListValue>(new ListValue()))));
}

void SpeechInputExtensionManager::DispatchEventToExtension(
    const std::string& extension_id, const std::string& event,
    scoped_ptr<ListValue> event_args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  if (profile_ && profile_->GetExtensionEventRouter()) {
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id, event, event_args.Pass(), profile_, GURL());
  }
}

void SpeechInputExtensionManager::DispatchError(
    const std::string& error, bool dispatch_event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string extension_id;
  {
    base::AutoLock auto_lock(state_lock_);
    if (state_ == kShutdown)
      return;

    extension_id = extension_id_in_use_;
    ResetToIdleState();

    // Will set the error property in the ongoing extension function calls.
    ExtensionError details(extension_id, error);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_FAILED,
        content::Source<Profile>(profile_),
        content::Details<ExtensionError>(&details));
  }

  // Used for errors that are also reported via the onError event.
  if (dispatch_event) {
    scoped_ptr<ListValue> args(new ListValue());
    DictionaryValue* js_error = new DictionaryValue();
    args->Append(js_error);
    js_error->SetString(kErrorCodeKey, error);
    DispatchEventToExtension(extension_id, kOnErrorEvent, args.Pass());
  }
}

bool SpeechInputExtensionManager::Start(
    const std::string& extension_id, const std::string& language,
    const std::string& grammar, bool filter_profanities, std::string* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);
  VLOG(1) << "Requesting start (UI thread)";

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown ||
      (!extension_id_in_use_.empty() && extension_id_in_use_ != extension_id)) {
    *error = kErrorRequestDenied;
    return false;
  }

  switch (state_) {
    case kIdle:
      break;

    case kStarting:
      *error = kErrorRequestInProgress;
      return false;

    case kRecording:
    case kStopping:
      *error = kErrorInvalidOperation;
      return false;

    default:
      NOTREACHED();
  }

  const extensions::Extension* extension = profile_->GetExtensionService()->
      GetExtensionById(extension_id, true);
  DCHECK(extension);
  const std::string& extension_name = extension->name();

  extension_id_in_use_ = extension_id;
  VLOG(1) << "State changed to starting";
  state_ = kStarting;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      profile_->GetRequestContext();

  const int render_process_id = GetRenderProcessIDForExtension(extension_id);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::StartOnIOThread, this,
                 url_request_context_getter, extension_name, language, grammar,
                 filter_profanities, render_process_id));
  return true;
}

void SpeechInputExtensionManager::StartOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    const std::string& extension_name,
    const std::string& language,
    const std::string& grammar,
    bool filter_profanities,
    int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "Requesting start (IO thread)";

  // Everything put inside the lock to ensure the validity of context_getter,
  // guaranteed while not in the shutdown state. Any ongoing or recognition
  // request will be requested to be aborted when entering the shutdown state.
  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  // TODO(primiano): These two checks below could be avoided, since they are
  // already handled in the speech recognition classes. However, since the
  // speech input extensions tests are bypassing the manager, we need them to
  // pass the tests.
  if (!GetSpeechInputExtensionInterface()->HasAudioInputDevices()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechInputExtensionManager::DispatchError, this,
                   std::string(kErrorNoRecordingDeviceFound), false));
    return;
  }

  if (GetSpeechInputExtensionInterface()->IsCapturingAudio()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechInputExtensionManager::DispatchError, this,
        std::string(kErrorRecordingDeviceInUse), false));
    return;
  }

  GetSpeechInputExtensionInterface()->StartRecording(this,
                                                     context_getter,
                                                     extension_name,
                                                     language,
                                                     grammar,
                                                     filter_profanities,
                                                     render_process_id);
}

bool SpeechInputExtensionManager::HasAudioInputDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return SpeechRecognitionManager::GetInstance()->HasAudioInputDevices();
}

bool SpeechInputExtensionManager::IsCapturingAudio() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return SpeechRecognitionManager::GetInstance()->IsCapturingAudio();
}

void SpeechInputExtensionManager::IsRecording(
    const IsRecordingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::IsRecordingOnIOThread,
                 this, callback));
}

void SpeechInputExtensionManager::IsRecordingOnIOThread(
    const IsRecordingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool result = GetSpeechInputExtensionInterface()->IsCapturingAudio();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::IsRecordingOnUIThread,
                 this, callback, result));
}

void SpeechInputExtensionManager::IsRecordingOnUIThread(
    const IsRecordingCallback& callback,
    bool result) {
  BrowserThread::CurrentlyOn(BrowserThread::UI);
  callback.Run(result);
}

void SpeechInputExtensionManager::StartRecording(
    content::SpeechRecognitionEventListener* listener,
    net::URLRequestContextGetter* context_getter,
    const std::string& extension_name,
    const std::string& language,
    const std::string& grammar,
    bool filter_profanities,
    int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  content::SpeechRecognitionSessionContext context;
  context.requested_by_page_element = false;
  context.render_process_id = render_process_id;
  context.context_name = extension_name;

  content::SpeechRecognitionSessionConfig config;
  config.is_one_shot = true;
  config.language = language;
  config.grammars.push_back(content::SpeechRecognitionGrammar(grammar));
  config.initial_context = context;
  config.url_request_context_getter = context_getter;
  config.filter_profanities = filter_profanities;
  config.event_listener = listener;

  DCHECK(!is_recognition_in_progress_);
  SpeechRecognitionManager& manager = *SpeechRecognitionManager::GetInstance();
  speech_recognition_session_id_ =
      manager.CreateSession(config);
  DCHECK_NE(speech_recognition_session_id_,
            SpeechRecognitionManager::kSessionIDInvalid);
  is_recognition_in_progress_ = true;
  manager.StartSession(speech_recognition_session_id_);
}

bool SpeechInputExtensionManager::HasValidRecognizer() {
  if (!is_recognition_in_progress_)
    return false;
  return SpeechRecognitionManager::GetInstance()->IsCapturingAudio();
}

bool SpeechInputExtensionManager::Stop(const std::string& extension_id,
    std::string* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);
  VLOG(1) << "Requesting stop (UI thread)";

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown ||
      (!extension_id_in_use_.empty() && extension_id_in_use_ != extension_id)) {
    *error = kErrorRequestDenied;
    return false;
  }

  switch (state_) {
    case kRecording:
      break;

    case kStopping:
      *error = kErrorRequestInProgress;
      return false;

    case kIdle:
    case kStarting:
      *error = kErrorInvalidOperation;
      return false;

    default:
      NOTREACHED();
  }

  // Guarded by the state lock.
  DCHECK(GetSpeechInputExtensionInterface()->HasValidRecognizer());

  VLOG(1) << "State changed to stopping";
  state_ = kStopping;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::ForceStopOnIOThread, this));
  return true;
}

void SpeechInputExtensionManager::ForceStopOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "Requesting forced stop (IO thread)";

  base::AutoLock auto_lock(state_lock_);
  DCHECK(state_ != kIdle);

  GetSpeechInputExtensionInterface()->StopRecording(false);

  if (state_ == kShutdown)
    return;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::StopSucceededOnUIThread, this));
}

void SpeechInputExtensionManager::StopRecording(bool recognition_failed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!is_recognition_in_progress_)
    return;
  DCHECK_NE(speech_recognition_session_id_,
            SpeechRecognitionManager::kSessionIDInvalid);
  SpeechRecognitionManager::GetInstance()->AbortSession(
      speech_recognition_session_id_);
}

void SpeechInputExtensionManager::StopSucceededOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Stop succeeded (UI thread)";

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  std::string extension_id = extension_id_in_use_;
  ResetToIdleState();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STOPPED,
      // Guarded by the state_ == kShutdown check.
      content::Source<Profile>(profile_),
      content::Details<std::string>(&extension_id));
}

void SpeechInputExtensionManager::OnAudioLevelsChange(int session_id,
                                                      float volume,
                                                      float noise_volume) {}

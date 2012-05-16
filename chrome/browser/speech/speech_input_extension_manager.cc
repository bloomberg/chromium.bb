// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_extension_manager.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/speech/speech_recognition_tray_icon_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognizer.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"

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

// Session id provided to the speech recognizer. Since only one extension can
// be recording on the same time a constant value is enough as id.
// TODO(primiano) this will not be valid anymore once speech input extension
// will use the SpeechRecognitionManager and not the SpeechRecognizer directly.
static const int kSpeechInputSessionId = 1;

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
  virtual void RegisterUserPrefs(PrefService* prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE { return false; }
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE { return true; }
  virtual bool ServiceIsCreatedWithProfile() OVERRIDE { return true; }

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

void SpeechInputExtensionManager::Factory::RegisterUserPrefs(
    PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSpeechInputTrayNotificationShown,
                             false,
                             PrefService::UNSYNCABLE_PREF);
}

SpeechInputExtensionInterface::SpeechInputExtensionInterface() {
}

SpeechInputExtensionInterface::~SpeechInputExtensionInterface() {
}

SpeechInputExtensionManager::SpeechInputExtensionManager(Profile* profile)
    : profile_(profile),
      state_(kIdle),
      registrar_(new content::NotificationRegistrar),
      speech_interface_(NULL) {
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
        content::Details<UnloadedExtensionInfo>(details)->extension->id());
  } else {
    NOTREACHED();
  }
}

void SpeechInputExtensionManager::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Profile shutting down.";

  base::AutoLock auto_lock(state_lock_);
  DCHECK(state_ != kShutdown);
  if (state_ != kIdle) {
    DCHECK(notification_.get());
    notification_->Hide();
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&SpeechInputExtensionManager::ForceStopOnIOThread, this));
  }
  state_ = kShutdown;
  VLOG(1) << "Entering the shutdown sink state.";
  registrar_.reset();
  profile_ = NULL;
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
    SpeechInputExtensionInterface* interface) {
  speech_interface_ = interface;
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

void SpeechInputExtensionManager::OnRecognitionResult(
    int session_id,
    const content::SpeechRecognitionResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, kSpeechInputSessionId);

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

  ListValue args;
  DictionaryValue* js_event = new DictionaryValue();
  args.Append(js_event);

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

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  VLOG(1) << "Results: " << json_args;
  DispatchEventToExtension(extension_id, kOnResultEvent, json_args);
}

void SpeechInputExtensionManager::OnRecognitionStart(int session_id) {
  DCHECK_EQ(session_id, kSpeechInputSessionId);
}

void SpeechInputExtensionManager::OnAudioStart(int session_id) {
  VLOG(1) << "OnAudioStart";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, kSpeechInputSessionId);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::DidStartReceivingAudioOnUIThread,
      this));
}

void SpeechInputExtensionManager::OnAudioEnd(int session_id) {
  DCHECK_EQ(session_id, kSpeechInputSessionId);
}

void SpeechInputExtensionManager::OnRecognitionEnd(int session_id) {
  DCHECK_EQ(session_id, kSpeechInputSessionId);
}

void SpeechInputExtensionManager::DidStartReceivingAudioOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  DCHECK_EQ(state_, kStarting);
  VLOG(1) << "State changed to recording";
  state_ = kRecording;

  const Extension* extension = profile_->GetExtensionService()->
      GetExtensionById(extension_id_in_use_, true);
  DCHECK(extension);

  bool show_notification = !profile_->GetPrefs()->GetBoolean(
      prefs::kSpeechInputTrayNotificationShown);

  if (!notification_.get())
    notification_ = new SpeechRecognitionTrayIconController();
  notification_->Show(UTF8ToUTF16(extension->name()), show_notification);

  if (show_notification) {
    profile_->GetPrefs()->SetBoolean(
        prefs::kSpeechInputTrayNotificationShown, true);
  }

  VLOG(1) << "Sending start notification";
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STARTED,
      content::Source<Profile>(profile_),
      content::Details<std::string>(&extension_id_in_use_));
}

void SpeechInputExtensionManager::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, kSpeechInputSessionId);

  // Simply return in case of an ERROR_ABORTED, since it is not contemplated
  // in the speech input extensions architecture.
  if (error.code == content::SPEECH_RECOGNITION_ERROR_ABORTED)
      return;

  VLOG(1) << "OnRecognitionError: " << error.code;

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  // Release the recognizer object.
  GetSpeechInputExtensionInterface()->StopRecording(true);

  std::string event_error_code;
  bool report_to_event = true;

  switch (error.code) {
    case content::SPEECH_RECOGNITION_ERROR_NONE:
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
  DCHECK_EQ(session_id, kSpeechInputSessionId);
}

void SpeechInputExtensionManager::OnSoundStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, kSpeechInputSessionId);
  VLOG(1) << "OnSoundStart";

  std::string json_args;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::DispatchEventToExtension,
      this, extension_id_in_use_, std::string(kOnSoundStartEvent),
      json_args));
}

void SpeechInputExtensionManager::OnSoundEnd(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, kSpeechInputSessionId);
  VLOG(1) << "OnSoundEnd";

  std::string json_args;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::DispatchEventToExtension,
      this, extension_id_in_use_, std::string(kOnSoundEndEvent),
      json_args));
}

void SpeechInputExtensionManager::DispatchEventToExtension(
    const std::string& extension_id, const std::string& event,
    const std::string& json_args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  if (profile_ && profile_->GetExtensionEventRouter()) {
    std::string final_args;
    if (json_args.empty()) {
      ListValue args;
      base::JSONWriter::Write(&args, &final_args);
    } else {
      final_args = json_args;
    }

    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id, event, final_args, profile_, GURL());
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

    if (state_ == kRecording) {
      DCHECK(notification_.get());
      notification_->Hide();
    }

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
    ListValue args;
    DictionaryValue* js_error = new DictionaryValue();
    args.Append(js_error);
    js_error->SetString(kErrorCodeKey, error);
    std::string json_args;
    base::JSONWriter::Write(&args, &json_args);
    DispatchEventToExtension(extension_id,
        kOnErrorEvent, json_args);
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

  extension_id_in_use_ = extension_id;
  VLOG(1) << "State changed to starting";
  state_ = kStarting;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::StartOnIOThread, this,
                 profile_->GetRequestContext(), language, grammar,
                 filter_profanities));
  return true;
}

void SpeechInputExtensionManager::StartOnIOThread(
    net::URLRequestContextGetter* context_getter,
    const std::string& language,
    const std::string& grammar,
    bool filter_profanities) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "Requesting start (IO thread)";

  // Everything put inside the lock to ensure the validity of context_getter,
  // guaranteed while not in the shutdown state. Any ongoing or recognition
  // request will be requested to be aborted when entering the shutdown state.
  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

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

  GetSpeechInputExtensionInterface()->StartRecording(
      this, context_getter, kSpeechInputSessionId, language, grammar,
      filter_profanities);
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
    int session_id,
    const std::string& language,
    const std::string& grammar,
    bool filter_profanities) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!recognizer_);
  recognizer_ = content::SpeechRecognizer::Create(
      listener, session_id, language, grammar, context_getter,
      filter_profanities, "", "");
  recognizer_->StartRecognition();
}

bool SpeechInputExtensionManager::HasValidRecognizer() {
  return !!recognizer_;
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
  if (recognizer_) {
    // Recognition is already cancelled in case of failure.
    // Double-cancelling leads to assertion failures.
    if (!recognition_failed)
      recognizer_->AbortRecognition();
    recognizer_.release();
  }
}

void SpeechInputExtensionManager::StopSucceededOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Stop succeeded (UI thread)";

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  std::string extension_id = extension_id_in_use_;
  ResetToIdleState();

  DCHECK(notification_.get());
  notification_->Hide();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STOPPED,
      // Guarded by the state_ == kShutdown check.
      content::Source<Profile>(profile_),
      content::Details<std::string>(&extension_id));
}

void SpeechInputExtensionManager::OnAudioLevelsChange(int session_id,
                                                       float volume,
                                                       float noise_volume) {
  DCHECK_EQ(session_id, kSpeechInputSessionId);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::SetInputVolumeOnUIThread,
      this, volume));
}

void SpeechInputExtensionManager::SetInputVolumeOnUIThread(
    float volume) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(notification_.get());
  notification_->SetVUMeterVolume(volume);
}

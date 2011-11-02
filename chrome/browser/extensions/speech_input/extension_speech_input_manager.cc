// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/speech_input/extension_speech_input_manager.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/speech_input/extension_speech_input_api_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

using namespace speech_input;

namespace constants = extension_speech_input_api_constants;

namespace {

// Caller id provided to the speech recognizer. Since only one extension can
// be recording on the same time a constant value is enough as id.
static const int kSpeechCallerId = 1;

// Wrap an ExtensionSpeechInputManager using scoped_refptr to avoid
// assertion failures on destruction because of not using release().
class ExtensionSpeechInputManagerWrapper : public ProfileKeyedService {
 public:
  explicit ExtensionSpeechInputManagerWrapper(
      ExtensionSpeechInputManager* manager)
      : manager_(manager) {}

  virtual ~ExtensionSpeechInputManagerWrapper() {}

  ExtensionSpeechInputManager* manager() const { return manager_.get(); }

 private:
  // Methods from ProfileKeyedService.
  virtual void Shutdown() OVERRIDE {
    manager()->ShutdownOnUIThread();
  }

  scoped_refptr<ExtensionSpeechInputManager> manager_;
};

}

// Factory for ExtensionSpeechInputManagers as profile keyed services.
class ExtensionSpeechInputManager::Factory : public ProfileKeyedServiceFactory {
 public:
  static void Initialize();
  static Factory* GetInstance();

  ExtensionSpeechInputManagerWrapper* GetForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<Factory>;

  Factory();
  virtual ~Factory();

  // ProfileKeyedServiceFactory methods:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE { return false; }
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE { return true; }
  virtual bool ServiceIsCreatedWithProfile() OVERRIDE { return true; }

  DISALLOW_COPY_AND_ASSIGN(Factory);
};

void ExtensionSpeechInputManager::Factory::Initialize() {
  GetInstance();
}

ExtensionSpeechInputManager::Factory*
    ExtensionSpeechInputManager::Factory::GetInstance() {
  return Singleton<ExtensionSpeechInputManager::Factory>::get();
}

ExtensionSpeechInputManagerWrapper*
    ExtensionSpeechInputManager::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionSpeechInputManagerWrapper*>(
      GetServiceForProfile(profile, true));
}

ExtensionSpeechInputManager::Factory::Factory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

ExtensionSpeechInputManager::Factory::~Factory() {
}

ProfileKeyedService*
    ExtensionSpeechInputManager::Factory::BuildServiceInstanceFor(
    Profile* profile) const {
  scoped_refptr<ExtensionSpeechInputManager> manager(
      new ExtensionSpeechInputManager(profile));
  return new ExtensionSpeechInputManagerWrapper(manager);
}

ExtensionSpeechInterface::ExtensionSpeechInterface() {
}

ExtensionSpeechInterface::~ExtensionSpeechInterface() {
}

ExtensionSpeechInputManager::ExtensionSpeechInputManager(Profile* profile)
    : profile_(profile),
      state_(kIdle),
      speech_interface_(NULL) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

ExtensionSpeechInputManager::~ExtensionSpeechInputManager() {
}

ExtensionSpeechInputManager* ExtensionSpeechInputManager::GetForProfile(
    Profile* profile) {
  ExtensionSpeechInputManagerWrapper *wrapper =
      Factory::GetInstance()->GetForProfile(profile);
  if (!wrapper)
    return NULL;
  return wrapper->manager();
}

void ExtensionSpeechInputManager::InitializeFactory() {
  Factory::Initialize();
}

void ExtensionSpeechInputManager::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    ExtensionUnloaded(
        content::Details<UnloadedExtensionInfo>(details)->extension->id());
  } else {
    NOTREACHED();
  }
}

void ExtensionSpeechInputManager::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Profile shutting down.";

  base::AutoLock auto_lock(state_lock_);
  DCHECK(state_ != kShutdown);
  if (state_ != kIdle) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&ExtensionSpeechInputManager::ForceStopOnIOThread, this));
  }
  state_ = kShutdown;
  VLOG(1) << "Entering the shutdown sink state.";
  registrar_.RemoveAll();
  profile_ = NULL;
}

void ExtensionSpeechInputManager::ExtensionUnloaded(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  VLOG(1) << "Extension unloaded. Requesting to enforce stop...";
  if (extension_id_in_use_ == extension_id) {
    if (state_ != kIdle) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
          base::Bind(&ExtensionSpeechInputManager::ForceStopOnIOThread, this));
    }
  }
}

void ExtensionSpeechInputManager::SetExtensionSpeechInterface(
    ExtensionSpeechInterface* interface) {
  speech_interface_ = interface;
}

ExtensionSpeechInterface*
    ExtensionSpeechInputManager::GetExtensionSpeechInterface() {
  return speech_interface_ ? speech_interface_ : this;
}

void ExtensionSpeechInputManager::ResetToIdleState() {
  VLOG(1) << "State changed to idle. Deassociating any extensions.";
  state_ = kIdle;
  extension_id_in_use_.clear();
}

void ExtensionSpeechInputManager::SetRecognitionResult(
    int caller_id,
    const SpeechInputResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(caller_id, kSpeechCallerId);

  // Stopping will start the disassociation with the extension.
  // Make a copy to report the results to the proper one.
  std::string extension_id = extension_id_in_use_;
  ForceStopOnIOThread();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ExtensionSpeechInputManager::SetRecognitionResultOnUIThread,
      this, result, extension_id));
}

void ExtensionSpeechInputManager::SetRecognitionResultOnUIThread(
    const SpeechInputResult& result, const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ListValue args;
  DictionaryValue* js_event = new DictionaryValue();
  args.Append(js_event);

  ListValue* js_hypothesis_array = new ListValue();
  js_event->Set(constants::kHypothesesKey, js_hypothesis_array);

  for (size_t i = 0; i < result.hypotheses.size(); ++i) {
    const SpeechInputHypothesis& hypothesis = result.hypotheses[i];

    DictionaryValue* js_hypothesis_object = new DictionaryValue();
    js_hypothesis_array->Append(js_hypothesis_object);

    js_hypothesis_object->SetString(constants::kUtteranceKey,
        UTF16ToUTF8(hypothesis.utterance));
    js_hypothesis_object->SetDouble(constants::kConfidenceKey,
        hypothesis.confidence);
  }

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  VLOG(1) << "Results: " << json_args;
  DispatchEventToExtension(extension_id, constants::kOnResultEvent, json_args);
}

void ExtensionSpeechInputManager::DidStartReceivingAudio(int caller_id) {
  VLOG(1) << "DidStartReceivingAudio";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(caller_id, kSpeechCallerId);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ExtensionSpeechInputManager::DidStartReceivingAudioOnUIThread,
      this));
}

void ExtensionSpeechInputManager::DidCompleteRecording(int caller_id) {
  DCHECK_EQ(caller_id, kSpeechCallerId);
}

void ExtensionSpeechInputManager::DidCompleteRecognition(int caller_id) {
  DCHECK_EQ(caller_id, kSpeechCallerId);
}

void ExtensionSpeechInputManager::DidStartReceivingAudioOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  DCHECK_EQ(state_, kStarting);
  VLOG(1) << "State changed to recording";
  state_ = kRecording;

  VLOG(1) << "Sending start notification";
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STARTED,
      content::Source<Profile>(profile_),
      content::Details<std::string>(&extension_id_in_use_));
}

void ExtensionSpeechInputManager::OnRecognizerError(
    int caller_id, SpeechInputError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(caller_id, kSpeechCallerId);
  VLOG(1) << "OnRecognizerError: " << error;

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  // Release the recognizer object.
  GetExtensionSpeechInterface()->StopRecording(true);

  std::string event_error_code;
  bool report_to_event = true;

  switch (error) {
    case kErrorNone:
      break;

    case kErrorAudio:
      if (state_ == kStarting) {
        event_error_code = constants::kErrorUnableToStart;
        report_to_event = false;
      } else {
        event_error_code = constants::kErrorCaptureError;
      }
      break;

    case kErrorNetwork:
      event_error_code = constants::kErrorNetworkError;
      break;

    case kErrorBadGrammar:
      // No error is returned on invalid language, for example.
      // To avoid confusion about when this is would be fired, the invalid
      // params error is not being exposed to the onError event.
      event_error_code = constants::kErrorUnableToStart;
      break;

    case kErrorNoSpeech:
      event_error_code = constants::kErrorNoSpeechHeard;
      break;

    case kErrorNoMatch:
      event_error_code = constants::kErrorNoResults;
      break;

    // The remaining kErrorAborted case should never be returned by the server.
    default:
      NOTREACHED();
  }

  if (!event_error_code.empty()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&ExtensionSpeechInputManager::DispatchError,
        this, event_error_code, report_to_event));
  }
}

void ExtensionSpeechInputManager::DidCompleteEnvironmentEstimation(
    int caller_id) {
  DCHECK_EQ(caller_id, kSpeechCallerId);
}

void ExtensionSpeechInputManager::DidStartReceivingSpeech(int caller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(caller_id, kSpeechCallerId);
  VLOG(1) << "DidStartReceivingSpeech";

  std::string json_args;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ExtensionSpeechInputManager::DispatchEventToExtension,
      this, extension_id_in_use_, std::string(constants::kOnSoundStartEvent),
      json_args));
}

void ExtensionSpeechInputManager::DidStopReceivingSpeech(int caller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(caller_id, kSpeechCallerId);
  VLOG(1) << "DidStopReceivingSpeech";

  std::string json_args;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ExtensionSpeechInputManager::DispatchEventToExtension,
      this, extension_id_in_use_, std::string(constants::kOnSoundEndEvent),
      json_args));
}

void ExtensionSpeechInputManager::DispatchEventToExtension(
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
      base::JSONWriter::Write(&args, false, &final_args);
    } else {
      final_args = json_args;
    }

    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id, event, final_args, profile_, GURL());
  }
}

void ExtensionSpeechInputManager::DispatchError(
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
    ListValue args;
    DictionaryValue *js_error = new DictionaryValue();
    args.Append(js_error);
    js_error->SetString(constants::kErrorCodeKey, error);
    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    DispatchEventToExtension(extension_id,
        constants::kOnErrorEvent, json_args);
  }
}

bool ExtensionSpeechInputManager::Start(const std::string& extension_id,
    const std::string& language, const std::string& grammar,
    bool filter_profanities, std::string* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);
  VLOG(1) << "Requesting start (UI thread)";

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown ||
      (!extension_id_in_use_.empty() && extension_id_in_use_ != extension_id)) {
    *error = constants::kErrorRequestDenied;
    return false;
  }

  switch (state_) {
    case kIdle:
      break;

    case kStarting:
      *error = constants::kErrorRequestInProgress;
      return false;

    case kRecording:
    case kStopping:
      *error = constants::kErrorInvalidOperation;
      return false;

    default:
      NOTREACHED();
  }

  extension_id_in_use_ = extension_id;
  VLOG(1) << "State changed to starting";
  state_ = kStarting;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&ExtensionSpeechInputManager::StartOnIOThread, this,
      profile_->GetRequestContext(), language, grammar, filter_profanities));
  return true;
}

void ExtensionSpeechInputManager::StartOnIOThread(
    net::URLRequestContextGetter* context_getter,
    const std::string& language, const std::string& grammar,
    bool filter_profanities) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "Requesting start (IO thread)";

  // Everything put inside the lock to ensure the validity of context_getter,
  // guaranteed while not in the shutdown state. Any ongoing or recognition
  // request will be requested to be aborted when entering the shutdown state.
  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown)
    return;

  if (!GetExtensionSpeechInterface()->HasAudioInputDevices()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&ExtensionSpeechInputManager::DispatchError, this,
        std::string(constants::kErrorNoRecordingDeviceFound), false));
    return;
  }

  if (GetExtensionSpeechInterface()->IsRecordingInProcess()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&ExtensionSpeechInputManager::DispatchError, this,
        std::string(constants::kErrorRecordingDeviceInUse), false));
    return;
  }

  GetExtensionSpeechInterface()->StartRecording(this, context_getter,
      kSpeechCallerId, language, grammar, filter_profanities);
}

bool ExtensionSpeechInputManager::HasAudioInputDevices() {
  return AudioManager::GetAudioManager()->HasAudioInputDevices();
}

bool ExtensionSpeechInputManager::IsRecordingInProcess() {
  // Thread-safe query.
  return AudioManager::GetAudioManager()->IsRecordingInProcess();
}

bool ExtensionSpeechInputManager::IsRecording() {
  return GetExtensionSpeechInterface()->IsRecordingInProcess();
}

void ExtensionSpeechInputManager::StartRecording(
    speech_input::SpeechRecognizerDelegate* delegate,
    net::URLRequestContextGetter* context_getter, int caller_id,
    const std::string& language, const std::string& grammar,
    bool filter_profanities) {
  DCHECK(!recognizer_);
  recognizer_ = new SpeechRecognizer(delegate, caller_id, language, grammar,
      context_getter, filter_profanities, "", "");
  recognizer_->StartRecording();
}

bool ExtensionSpeechInputManager::HasValidRecognizer() {
  // Conditional expression used to avoid a performance warning on windows.
  return recognizer_ ? true : false;
}

bool ExtensionSpeechInputManager::Stop(const std::string& extension_id,
    std::string* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);
  VLOG(1) << "Requesting stop (UI thread)";

  base::AutoLock auto_lock(state_lock_);
  if (state_ == kShutdown ||
      (!extension_id_in_use_.empty() && extension_id_in_use_ != extension_id)) {
    *error = constants::kErrorRequestDenied;
    return false;
  }

  switch (state_) {
    case kRecording:
      break;

    case kStopping:
      *error = constants::kErrorRequestInProgress;
      return false;

    case kIdle:
    case kStarting:
      *error = constants::kErrorInvalidOperation;
      return false;

    default:
      NOTREACHED();
  }

  // Guarded by the state lock.
  DCHECK(GetExtensionSpeechInterface()->HasValidRecognizer());

  VLOG(1) << "State changed to stopping";
  state_ = kStopping;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&ExtensionSpeechInputManager::ForceStopOnIOThread, this));
  return true;
}

void ExtensionSpeechInputManager::ForceStopOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "Requesting forced stop (IO thread)";

  base::AutoLock auto_lock(state_lock_);
  DCHECK(state_ != kIdle);

  GetExtensionSpeechInterface()->StopRecording(false);

  if (state_ == kShutdown)
    return;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ExtensionSpeechInputManager::StopSucceededOnUIThread, this));
}

void ExtensionSpeechInputManager::StopRecording(bool recognition_failed) {
  if (recognizer_) {
    // Recognition is already cancelled in case of failure.
    // Double-cancelling leads to assertion failures.
    if (!recognition_failed)
      recognizer_->CancelRecognition();
    recognizer_.release();
  }
}

void ExtensionSpeechInputManager::StopSucceededOnUIThread() {
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

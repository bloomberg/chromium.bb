// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_

#include <queue>
#include <string>

#include "base/memory/singleton.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_tts_api_util.h"

// Abstract class that defines the native platform TTS interface.
class ExtensionTtsPlatformImpl {
 public:
  static ExtensionTtsPlatformImpl* GetInstance();

  // Speak the given utterance with the given parameters if possible,
  // and return true on success. Utterance will always be nonempty.
  // If the user does not specify the other values, then locale and gender
  // will be empty strings, and rate, pitch, and volume will be -1.0.
  //
  // The ExtensionTtsController will only try to speak one utterance at
  // a time. If it wants to interrupt speech, it will always call Stop
  // before speaking again, otherwise it will wait until IsSpeaking
  // returns false before calling Speak again.
  virtual bool Speak(
      const std::string& utterance,
      const std::string& locale,
      const std::string& gender,
      double rate,
      double pitch,
      double volume) = 0;

  // Stop speaking immediately and return true on success.
  virtual bool StopSpeaking() = 0;

  // Return true if the synthesis engine is currently speaking.
  virtual bool IsSpeaking() = 0;

  virtual std::string error();
  virtual void clear_error();
  virtual void set_error(const std::string& error);

 protected:
  ExtensionTtsPlatformImpl() {}
  virtual ~ExtensionTtsPlatformImpl() {}

  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImpl);
};

// One speech utterance.
class Utterance {
 public:
  // Construct an utterance given a profile, the text to speak,
  // the options passed to tts.speak, and a completion task to call
  // when the utterance is done speaking.
  Utterance(Profile* profile,
            const std::string& text,
            DictionaryValue* options,
            Task* completion_task);
  ~Utterance();

  // Calls the completion task and then destroys itself.
  void FinishAndDestroy();

  void set_error(const std::string& error) { error_ = error; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  // Accessors
  Profile* profile() { return profile_; }
  const std::string& extension_id() { return extension_id_; }
  int id() { return id_; }
  const std::string& text() { return text_; }
  const Value* options() { return options_.get(); }
  const std::string& voice_name() { return voice_name_; }
  const std::string& locale() { return locale_; }
  const std::string& gender() { return gender_; }
  double rate() { return rate_; }
  double pitch() { return pitch_; }
  double volume() { return volume_; }
  bool can_enqueue() { return can_enqueue_; }
  const std::string& error() { return error_; }

 private:
  // The profile that initiated this utterance.
  Profile* profile_;

  // The extension ID of the extension providing TTS for this utterance, or
  // empty if native TTS is being used.
  std::string extension_id_;

  // The unique ID of this utterance, used to associate callback functions
  // with utterances.
  int id_;

  // The id of the next utterance, so we can associate requests with
  // responses.
  static int next_utterance_id_;

  // The text to speak.
  std::string text_;

  // The full options arg passed to tts.speak, which may include fields
  // other than the ones we explicitly parse, below.
  scoped_ptr<Value> options_;

  // The parsed options.
  std::string voice_name_;
  std::string locale_;
  std::string gender_;
  double rate_;
  double pitch_;
  double volume_;
  bool can_enqueue_;

  // The error string to pass to the completion task. Will be empty if
  // no error occurred.
  std::string error_;

  // The method to call when this utterance has completed speaking.
  Task* completion_task_;
};

// Singleton class that manages text-to-speech.
class ExtensionTtsController {
 public:
  // Get the single instance of this class.
  static ExtensionTtsController* GetInstance();

  // Returns true if we're currently speaking an utterance.
  bool IsSpeaking() const;

  // Speak the given utterance. If the utterance's can_enqueue flag is true
  // and another utterance is in progress, adds it to the end of the queue.
  // Otherwise, interrupts any current utterance and speaks this one
  // immediately.
  void SpeakOrEnqueue(Utterance* utterance);

  // Stop all utterances and flush the queue.
  void Stop();

  // Called when an extension finishes speaking an utterance.
  void OnSpeechFinished(int request_id, const std::string& error_message);

  // For unit testing.
  void SetPlatformImpl(ExtensionTtsPlatformImpl* platform_impl);

 private:
  ExtensionTtsController();
  virtual ~ExtensionTtsController();

  // Get the platform TTS implementation (or injected mock).
  ExtensionTtsPlatformImpl* GetPlatformImpl();

  // Start speaking the given utterance. Will either take ownership of
  // |utterance| or delete it if there's an error.
  void SpeakNow(Utterance* utterance);

  // Called periodically when speech is ongoing. Checks to see if the
  // underlying platform speech system has finished the current utterance,
  // and if so finishes it and pops the next utterance off the queue.
  void CheckSpeechStatus();

  // Clear the utterance queue.
  void ClearUtteranceQueue();

  // Finalize and delete the current utterance.
  void FinishCurrentUtterance();

  // Start speaking the next utterance in the queue.
  void SpeakNextUtterance();

  // Return the id string of the first extension with tts_voices in its
  // manifest that matches the speech parameters of this utterance,
  // or the empty string if none is found.
  std::string GetMatchingExtensionId(Utterance* utterance);

  ScopedRunnableMethodFactory<ExtensionTtsController> method_factory_;
  friend struct DefaultSingletonTraits<ExtensionTtsController>;

  // The current utterance being spoken.
  Utterance* current_utterance_;

  // A queue of utterances to speak after the current one finishes.
  std::queue<Utterance*> utterance_queue_;

  // A pointer to the platform implementation of text-to-speech, for
  // dependency injection.
  ExtensionTtsPlatformImpl* platform_impl_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsController);
};

//
// Extension API function definitions
//

class ExtensionTtsSpeakFunction : public AsyncExtensionFunction {
 private:
  ~ExtensionTtsSpeakFunction() {}
  virtual bool RunImpl();
  void SpeechFinished();
  Utterance* utterance_;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.tts.speak")
};

class ExtensionTtsStopSpeakingFunction : public SyncExtensionFunction {
 private:
  ~ExtensionTtsStopSpeakingFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.tts.stop")
};

class ExtensionTtsIsSpeakingFunction : public SyncExtensionFunction {
 private:
  ~ExtensionTtsIsSpeakingFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.tts.isSpeaking")
};

class ExtensionTtsSpeakCompletedFunction : public SyncExtensionFunction {
 private:
  ~ExtensionTtsSpeakCompletedFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.tts.speakCompleted")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_

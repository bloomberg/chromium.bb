// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_

#include <queue>

#include "base/singleton.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_tts_api_util.h"

// Abstract class that defines the native platform TTS interface.
class ExtensionTtsPlatformImpl {
 public:
  static ExtensionTtsPlatformImpl* GetInstance();

  // Speak the given utterance with the given parameters if possible,
  // and return true on success. Utterance will always be nonempty.
  // If the user does not specify the other values, language and gender
  // will be empty strings, and rate, pitch, and volume will be -1.0.
  //
  // The ExtensionTtsController will only try to speak one utterance at
  // a time. If it wants to intterupt speech, it will always call Stop
  // before speaking again, otherwise it will wait until IsSpeaking
  // returns false before calling Speak again.
  virtual bool Speak(
      const std::string& utterance,
      const std::string& language,
      const std::string& gender,
      double rate,
      double pitch,
      double volume) = 0;

  // Stop speaking immediately and return true on success.
  virtual bool StopSpeaking() = 0;

  // Return true if the synthesis engine is currently speaking.
  virtual bool IsSpeaking() = 0;

  virtual std::string error() { return error_; }
  virtual void clear_error() { error_ = std::string(); }
  virtual void set_error(const std::string& error) { error_ = error; }

 protected:
  ExtensionTtsPlatformImpl() {}
  virtual ~ExtensionTtsPlatformImpl() {}

  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImpl);
};

// Singleton class that manages text-to-speech.
class ExtensionTtsController {
 public:
  // Get the single instance of this class.
  static ExtensionTtsController* GetInstance();

  struct Utterance {
    Utterance()
        : rate(-1.0),
          pitch(-1.0),
          volume(-1.0),
          success_task(NULL),
          failure_task(NULL) {
    }

    std::string text;
    std::string language;
    std::string gender;
    double rate;
    double pitch;
    double volume;

    Task* success_task;
    Task* failure_task;

    std::string error;
  };

  // Returns true if we're currently speaking an utterance.
  bool IsSpeaking() const;

  // Speak the given utterance. If |can_enqueue| is true and another
  // utterance is in progress, adds it to the end of the queue. Otherwise,
  // interrupts any current utterance and speaks this one immediately.
  void SpeakOrEnqueue(Utterance* utterance, bool can_enqueue);

  // Stop all utterances and flush the queue.
  void Stop();

  // For unit testing.
  void SetPlatformImpl(ExtensionTtsPlatformImpl* platform_impl);

 private:
  ExtensionTtsController();
  virtual ~ExtensionTtsController() {}

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
  void SpeechFinished(bool success);
  ExtensionTtsController::Utterance* utterance_;
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

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_

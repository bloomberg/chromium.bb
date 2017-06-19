// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"

class Profile;

namespace safe_browsing {

// Delegate class that provides services to the ChromeCleanerController class
// and can be overridden by tests via
// SetChromeCleanerControllerDelegateForTesting().
class ChromeCleanerControllerDelegate {
 public:
  using FetchedCallback = base::OnceCallback<void(base::FilePath)>;

  ChromeCleanerControllerDelegate();
  virtual ~ChromeCleanerControllerDelegate();

  // Fetches and verifies the Chrome Cleaner binary and passes the name of the
  // executable to |fetched_callback|. The file name will have the ".exe"
  // extension. If the operation fails, the file name passed to
  // |fecthed_callback| will be empty.
  virtual void FetchAndVerifyChromeCleaner(FetchedCallback fetched_callback);
  virtual bool SafeBrowsingExtendedReportingScoutEnabled();
  virtual bool IsMetricsAndCrashReportingEnabled();

  // Auxiliary methods for tagging and resetting open profiles.
  virtual void TagForResetting(Profile* profile);
  virtual void ResetTaggedProfiles(std::vector<Profile*> profiles,
                                   base::OnceClosure continuation);
};

// Controller class that keeps track of the execution of the Chrome Cleaner and
// the various states through which the execution will transition. Observers can
// register themselves to be notified of state changes. Intended to be used by
// the Chrome Cleaner webui page and the Chrome Cleaner prompt dialog.
//
// This class lives on, and all its members should be called only on, the UI
// thread.
class ChromeCleanerController {
 public:
  enum class State {
    // The default state where there is no Chrome Cleaner process or IPC to keep
    // track of and a reboot of the machine is not required to complete previous
    // cleaning attempts. This is also the state the controller will end up in
    // if any errors occur during execution of the Chrome Cleaner process.
    kIdle,
    // All steps up to and including scanning the machine occur in this
    // state. The steps include downloading the Chrome Cleaner binary, setting
    // up an IPC between Chrome and the Cleaner process, and the actual
    // scanning done by the Cleaner.
    kScanning,
    // Scanning has been completed and harmful or unwanted software was
    // found. In this state, the controller is waiting to get a response from
    // the user on whether or not they want the cleaner to remove the harmful
    // software that was found.
    kInfected,
    // The Cleaner process is cleaning the machine.
    kCleaning,
    // The cleaning has finished, but a reboot of the machine is required to
    // complete cleanup. This state will persist across restarts of Chrome until
    // a reboot is detected.
    kRebootRequired,
  };

  enum class IdleReason {
    kInitial,
    kScanningFoundNothing,
    kScanningFailed,
    kConnectionLost,
    kUserDeclinedCleanup,
    kCleaningFailed,
    kCleaningSucceeded,
  };

  enum class UserResponse {
    // User accepted the cleanup operation.
    kAccepted,
    // User explicitly denied the cleanup operation, for example by clicking the
    // Cleaner dialog's cancel button.
    kDenied,
    // The cleanup operation was denied when the user dismissed the Cleaner
    // dialog, for example by pressing the ESC key.
    kDismissed,
  };

  class Observer {
   public:
    virtual void OnIdle(IdleReason idle_reason) {}
    virtual void OnScanning() {}
    virtual void OnInfected(const std::set<base::FilePath>& files_to_delete) {}
    virtual void OnCleaning(const std::set<base::FilePath>& files_to_delete) {}
    virtual void OnRebootRequired() {}
    virtual void OnRebootFailed() {}

   protected:
    virtual ~Observer() = default;
  };

  // Returns the global controller object.
  static ChromeCleanerController* GetInstance();

  // Returns whether the Cleanup card in settings should be displayed.
  // Static to prevent instantiation of the global controller object.
  static bool ShouldShowCleanupInSettingsUI();

  State state() const { return state_; }

  // |AddObserver()| immediately notifies |observer| of the controller's state
  // by calling the corresponding |On*()| function.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Downloads the Chrome Cleaner binary, executes it and waits for the Cleaner
  // to communicate with Chrome about harmful software found on the
  // system. During this time, the controller will be in the kScanning state. If
  // any of the steps fail or if the Cleaner does not find harmful software on
  // the system, the controller will transition to the kIdle state, passing to
  // observers the reason for the transition. Otherwise, the scanner will
  // transition to the kInfected state.
  //
  // |reporter_invocation| is the invocation that was used to run the reporter
  // which found possible harmful software on the system.
  //
  // A call to Scan() will be a no-op if the controller is not in the kIdle
  // state. This gracefully handles cases where multiple user responses are
  // received, for example if a user manages to click on a "Scan" button
  // multiple times.
  void Scan(const SwReporterInvocation& reporter_invocation);

  // Sends the user's response, as to whether or not they want the Chrome
  // Cleaner to remove harmful software that was found, to the Chrome Cleaner
  // process. If the user accepted the prompt, then tags |profile| for
  // post-cleanup settings reset.
  //
  // A call to ReplyWithUserResponse() will be a no-op if the controller is not
  // in the kInfected state. This gracefully handles cases where multiple user
  // responses are received, for example if a user manages to click on a
  // "Cleanup" button multiple times.
  void ReplyWithUserResponse(Profile* profile, UserResponse user_response);

  // If the controller is in the kRebootRequired state, initiates a reboot of
  // the computer. Call this after obtaining permission from the user to
  // reboot.
  //
  // If initiating the reboot fails, observers will be notified via a call to
  // OnRebootFailed().
  //
  // Note that there are no guarantees that the reboot will in fact happen even
  // if the system calls to initiate a reboot return success.
  void Reboot();

  // Passing in a nullptr as |delegate| resets the delegate to a default
  // production version.
  void SetDelegateForTesting(ChromeCleanerControllerDelegate* delegate);
  void DismissRebootForTesting();

 private:
  ChromeCleanerController();
  ~ChromeCleanerController();

  void NotifyObserver(Observer* observer) const;
  void SetStateAndNotifyObservers(State state);
  // Used to invalidate weak pointers and reset accumulated data that is no
  // longer needed when entering the kIdle or kRebootRequired states.
  void ResetCleanerDataAndInvalidateWeakPtrs();

  // Callback that is called when the Chrome Cleaner binary has been fetched and
  // verified. An empty |executable_path| signals failure. A non-empty
  // |executable_path| must have the '.exe' file extension.
  void OnChromeCleanerFetchedAndVerified(base::FilePath executable_path);

  // Callback that checks if the weak pointer |controller| is still valid, and
  // if so will call OnPromptuser(). If |controller| is no longer valid, will
  // immediately send a Mojo response denying the cleanup operation.
  //
  // The other Mojo callbacks below do not need corresponding "weak" callbacks,
  // because for those cases nothing needs to be done if the weak pointer
  // referencing the controller instance is no longer valid (Chrome's Callback
  // objects become no-ops if the bound weak pointer is not valid).
  static void WeakOnPromptUser(
      const base::WeakPtr<ChromeCleanerController>& controller,
      std::unique_ptr<std::set<base::FilePath>> files_to_delete,
      chrome_cleaner::mojom::ChromePrompt::PromptUserCallback
          prompt_user_callback);

  void OnPromptUser(std::unique_ptr<std::set<base::FilePath>> files_to_delete,
                    chrome_cleaner::mojom::ChromePrompt::PromptUserCallback
                        prompt_user_callback);
  void OnConnectionClosed();
  void OnCleanerProcessDone(ChromeCleanerRunner::ProcessStatus process_status);
  void InitiateReboot();

  // Invoked once settings reset is done for tagged profiles.
  void OnSettingsResetCompleted();

  std::unique_ptr<ChromeCleanerControllerDelegate> real_delegate_;
  // Pointer to either real_delegate_ or one set by tests.
  ChromeCleanerControllerDelegate* delegate_;

  State state_ = State::kIdle;
  IdleReason idle_reason_ = IdleReason::kInitial;
  std::unique_ptr<SwReporterInvocation> reporter_invocation_;
  std::unique_ptr<std::set<base::FilePath>> files_to_delete_;
  // The Mojo callback that should be called to send a response to the Chrome
  // Cleaner process. This must be posted to run on the IO thread.
  chrome_cleaner::mojom::ChromePrompt::PromptUserCallback prompt_user_callback_;

  base::ObserverList<Observer> observer_list_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ChromeCleanerController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerController);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_

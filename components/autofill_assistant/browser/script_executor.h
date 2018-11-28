// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
// Class to execute an assistant script.
class ScriptExecutor : public ActionDelegate {
 public:
  // Listens to events on ScriptExecutor.
  // TODO(b/806868): Make global_payload a part of callback instead of the
  // listener.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Called when new server payloads are available.
    //
    // TODO(b/806868): Stop reporting the script payload once the server has
    // transitioned to global payloads.
    virtual void OnServerPayloadChanged(const std::string& global_payload,
                                        const std::string& script_payload) = 0;
  };

  // |delegate|, |listener|, |script_state| and |ordered_interrupts| should
  // outlive this object and should not be nullptr.
  ScriptExecutor(const std::string& script_path,
                 const std::string& global_payload,
                 const std::string& script_payload,
                 ScriptExecutor::Listener* listener,
                 std::map<std::string, ScriptStatusProto>* scripts_state,
                 const std::vector<Script*>* ordered_interrupts,
                 ScriptExecutorDelegate* delegate);
  ~ScriptExecutor() override;

  // What should happen after the script has run.
  enum AtEnd {
    // Continue normally.
    CONTINUE = 0,

    // Shut down Autofill Assistant.
    SHUTDOWN,

    // Shut down Autofill Assistant after a delay.
    SHUTDOWN_GRACEFULLY,

    // Shut down Autofill Assistant and CCT.
    CLOSE_CUSTOM_TAB,

    // Reset all state and restart.
    RESTART
  };

  // Contains the result of the Run operation.
  struct Result {
    bool success = false;
    AtEnd at_end = AtEnd::CONTINUE;
    std::vector<Selector> touchable_elements;

    Result();
    Result(const Result& other);
    ~Result();
  };

  using RunScriptCallback = base::OnceCallback<void(const Result&)>;
  void Run(RunScriptCallback callback);

  // Override ActionDelegate:
  std::unique_ptr<BatchElementChecker> CreateBatchElementChecker() override;
  void ShortWaitForElementExist(
      const Selector& selector,
      base::OnceCallback<void(bool)> callback) override;
  void WaitForElementVisible(base::TimeDelta max_wait_time,
                             bool allow_interrupt,
                             const Selector& selector,
                             base::OnceCallback<void(bool)> callback) override;
  void ShowStatusMessage(const std::string& message) override;
  void ClickOrTapElement(const Selector& selector,
                         base::OnceCallback<void(bool)> callback) override;
  void GetPaymentInformation(
      payments::mojom::PaymentOptionsPtr payment_options,
      base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
      const std::string& title,
      const std::vector<std::string>& supported_basic_card_networks) override;
  void Choose(const std::vector<UiController::Choice>& choice,
              base::OnceCallback<void(const std::string&)> callback) override;
  void ForceChoose(const std::string&) override;
  void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) override;
  void FillAddressForm(const autofill::AutofillProfile* profile,
                       const Selector& selector,
                       base::OnceCallback<void(bool)> callback) override;
  void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) override;
  void FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                    const base::string16& cvc,
                    const Selector& selector,
                    base::OnceCallback<void(bool)> callback) override;
  void SelectOption(const Selector& selector,
                    const std::string& selected_option,
                    base::OnceCallback<void(bool)> callback) override;
  void HighlightElement(const Selector& selector,
                        base::OnceCallback<void(bool)> callback) override;
  void FocusElement(const Selector& selector,
                    base::OnceCallback<void(bool)> callback) override;
  void SetTouchableElements(
      const std::vector<Selector>& element_selectors) override;
  void SetFieldValue(const Selector& selector,
                     const std::string& value,
                     bool simulate_key_presses,
                     base::OnceCallback<void(bool)> callback) override;
  void SetAttribute(const Selector& selector,
                    const std::vector<std::string>& attribute,
                    const std::string& value,
                    base::OnceCallback<void(bool)> callback) override;
  void SendKeyboardInput(const Selector& selector,
                         const std::string& text,
                         base::OnceCallback<void(bool)> callback) override;
  void GetOuterHtml(
      const Selector& selector,
      base::OnceCallback<void(bool, const std::string&)> callback) override;
  void LoadURL(const GURL& url) override;
  void Shutdown() override;
  void Close() override;
  void Restart() override;
  ClientMemory* GetClientMemory() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  content::WebContents* GetWebContents() override;
  void StopCurrentScriptAndShutdown(const std::string& message) override;
  void HideDetails() override;
  void ShowDetails(const DetailsProto& details,
                   base::OnceCallback<void(bool)> callback) override;
  void ShowProgressBar(int progress, const std::string& message) override;
  void HideProgressBar() override;
  void ShowOverlay() override;
  void HideOverlay() override;

 private:
  // Helper for WaitForElementVisible that keeps track of the state required to
  // run interrupts while waiting for a specific element.
  class WaitWithInterrupts : public ScriptExecutor::Listener {
   public:
    // Let the caller know about either the result of looking for the element or
    // of an abnormal result from an interrupt.
    //
    // If the given result is non-null, it should be forwarded as the result of
    // the main script.
    using Callback =
        base::OnceCallback<void(bool, const ScriptExecutor::Result*)>;

    // |main_script_| must not be null and outlive this instance.
    WaitWithInterrupts(ScriptExecutor* main_script,
                       base::TimeDelta max_wait_time,
                       ElementCheckType check_type,
                       const Selector& selectors,
                       WaitWithInterrupts::Callback callback);
    ~WaitWithInterrupts() override;

    void Run();
    void Shutdown();

   private:
    // Implements ScriptExecutor::Listener
    void OnServerPayloadChanged(const std::string& global_payload,
                                const std::string& script_payload) override;

    void OnPreconditionCheckDone(const Script* interrupt,
                                 bool precondition_match);
    void OnElementCheckDone(bool found);
    void OnTryDone();
    void OnAllDone();
    void RunInterrupt(const Script* interrupt);
    void OnInterruptDone(const ScriptExecutor::Result& result);
    void RunCallback(bool found, const ScriptExecutor::Result* result);

    // Saves the current state and sets save_pre_interrupt_state_.
    void SavePreInterruptState();

    // Restores the UI states as found by SavePreInterruptState.
    void RestorePreInterruptUiState();

    // if save_pre_interrupt_state_ is set, attempt to scroll the page back to
    // the original area.
    void RestorePreInterruptScroll(bool element_found);

    ScriptExecutor* main_script_;
    const base::TimeDelta max_wait_time_;
    const ElementCheckType check_type_;
    const Selector selector_;
    WaitWithInterrupts::Callback callback_;

    std::unique_ptr<BatchElementChecker> batch_element_checker_;
    std::set<const Script*> runnable_interrupts_;
    bool element_found_;

    // An empty vector of interrupts that can be passed to interrupt_executor_
    // and outlives it. Interrupts must not run interrupts.
    const std::vector<Script*> no_interrupts_;

    // The interrupt that's currently running.
    std::unique_ptr<ScriptExecutor> interrupt_executor_;

    // If true, pre-interrupt state was saved already. This happens just before
    // the first interrupt.
    bool saved_pre_interrupt_state_;

    // The status message that was displayed when the interrupt started.
    std::string pre_interrupt_status_;

    DISALLOW_COPY_AND_ASSIGN(WaitWithInterrupts);
  };
  friend class WaitWithInterrupts;

  void OnGetActions(bool result, const std::string& response);
  void ReportPayloadsToListener();
  void RunCallback(bool success);
  void RunCallbackWithResult(const Result& result);
  void ProcessNextAction();
  void ProcessAction(Action* action);
  void GetNextActions();
  void OnProcessedAction(std::unique_ptr<ProcessedActionProto> action);
  void WaitForElement(base::TimeDelta max_wait_time,
                      ElementCheckType check_type,
                      const Selector& selectors,
                      base::OnceCallback<void(bool)> callback);
  void OnWaitForElement(base::OnceCallback<void(bool)> callback);
  void OnWaitForElementVisible(
      base::OnceCallback<void(bool)> element_found_callback,
      bool element_found,
      const Result* interrupt_result);
  void OnChosen(base::OnceCallback<void(const std::string&)> callback,
                const std::string& chosen);

  std::string script_path_;
  std::string last_global_payload_;
  const std::string initial_script_payload_;
  std::string last_script_payload_;
  ScriptExecutor::Listener* const listener_;
  ScriptExecutorDelegate* delegate_;
  RunScriptCallback callback_;

  std::vector<std::unique_ptr<Action>> actions_;
  std::vector<ProcessedActionProto> processed_actions_;
  AtEnd at_end_;
  bool should_stop_script_;
  bool should_clean_contextual_ui_on_finish_;
  ActionProto::ActionInfoCase previous_action_type_;
  Selector last_focused_element_selector_;
  std::vector<Selector> touchable_elements_;
  std::map<std::string, ScriptStatusProto>* scripts_state_;
  std::unique_ptr<BatchElementChecker> batch_element_checker_;

  // Set of interrupts that might run during wait for dom actions with
  // allow_interrupt. Sorted by priority; an interrupt that appears on the
  // vector first should run first.
  const std::vector<Script*>* ordered_interrupts_;

  std::unique_ptr<WaitWithInterrupts> wait_with_interrupts_;

  base::WeakPtrFactory<ScriptExecutor> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ScriptExecutor);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/element_area.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_tracker.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace base {
class TickClock;
}  // namespace base

namespace autofill_assistant {
class ControllerTest;

// Autofill assistant controller controls autofill assistant action detection,
// display, execution and so on. The instance of this object self deletes when
// the web contents is being destroyed.
class Controller : public ScriptExecutorDelegate,
                   public virtual UiDelegate,
                   public ScriptTracker::Listener,
                   private content::WebContentsObserver,
                   public UserModel::Observer {
 public:
  // |web_contents|, |client| and |tick_clock| must remain valid for the
  // lifetime of the instance. Controller will take ownership of |service| if
  // specified, otherwise will create and own the default service.
  Controller(content::WebContents* web_contents,
             Client* client,
             const base::TickClock* tick_clock,
             std::unique_ptr<Service> service);
  ~Controller() override;

  // Let the controller know it should keep tracking script availability for the
  // current domain, to be ready to report any scripts as action.
  //
  // Activates the controller, if needed and runs it in the background, without
  // showing any UI until a script is started or Start() is called.
  //
  // Only the context of the first call to Track() is taken into account.
  //
  // If non-null |on_first_check_done| is called once the result of the first
  // check of script availability are in - whether they're positive or negative.
  void Track(std::unique_ptr<TriggerContext> trigger_context,
             base::OnceCallback<void()> on_first_check_done);

  // Returns true if we are in tracking mode and the first round of script
  // checks has been completed.
  bool HasRunFirstCheck() const;

  // Called when autofill assistant should start.
  //
  // This shows a UI, containing a progress bar, and executes the first
  // available autostartable script. Starts checking for scripts, if necessary.
  //
  // Start() does nothing if called more than once or if a script is already
  // running.
  //
  // Start() will overwrite any context previously set by Track().
  bool Start(const GURL& deeplink_url,
             std::unique_ptr<TriggerContext> trigger_context);

  // Returns true if the controller is in a state where UI is necessary.
  bool NeedsUI() const { return needs_ui_; }

  // Overrides ScriptExecutorDelegate:
  const ClientSettings& GetSettings() override;
  const GURL& GetCurrentURL() override;
  const GURL& GetDeeplinkURL() override;
  const GURL& GetScriptURL() override;
  Service* GetService() override;
  WebController* GetWebController() override;
  const TriggerContext* GetTriggerContext() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  WebsiteLoginManager* GetWebsiteLoginManager() override;
  content::WebContents* GetWebContents() override;
  std::string GetEmailAddressForAccessTokenAccount() override;
  std::string GetLocale() override;

  void SetTouchableElementArea(const ElementAreaProto& area) override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetBubbleMessage(const std::string& message) override;
  std::string GetBubbleMessage() const override;
  void SetDetails(std::unique_ptr<Details> details) override;
  void SetInfoBox(const InfoBox& info_box) override;
  void ClearInfoBox() override;
  void SetProgress(int progress) override;
  void SetProgressVisible(bool visible) override;
  void SetUserActions(
      std::unique_ptr<std::vector<UserAction>> user_actions) override;
  void SetViewportMode(ViewportMode mode) override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
  bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) override;
  bool IsNavigatingToNewDocument() override;
  bool HasNavigationError() override;
  void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(bool,
                              ProcessedActionStatusProto,
                              const UserModel*)> end_action_callback) override;
  void ClearGenericUi() override;

  // Show the UI if it's not already shown. This is only meaningful while in
  // states where showing the UI is optional, such as RUNNING, in tracking mode.
  void RequireUI() override;

  void AddListener(NavigationListener* listener) override;
  void RemoveListener(NavigationListener* listener) override;

  void SetExpandSheetForPromptAction(bool expand) override;
  void SetBrowseDomainsWhitelist(std::vector<std::string> domains) override;

  bool EnterState(AutofillAssistantState state) override;
  void SetCollectUserDataOptions(CollectUserDataOptions* options) override;
  void WriteUserData(
      base::OnceCallback<void(UserData*, UserData::FieldChange*)>) override;
  void WriteUserModel(
      base::OnceCallback<void(UserModel*)> write_callback) override;
  void OnScriptError(const std::string& error_message,
                     Metrics::DropOutReason reason);

  // Overrides autofill_assistant::UiDelegate:
  AutofillAssistantState GetState() override;
  void OnUserInteractionInsideTouchableArea() override;
  const Details* GetDetails() const override;
  const InfoBox* GetInfoBox() const override;
  int GetProgress() const override;
  bool GetProgressVisible() const override;
  const std::vector<UserAction>& GetUserActions() const override;
  bool PerformUserActionWithContext(
      int index,
      std::unique_ptr<TriggerContext> context) override;
  std::string GetDebugContext() override;
  const CollectUserDataOptions* GetCollectUserDataOptions() const override;
  const UserData* GetUserData() const override;
  void SetShippingAddress(
      std::unique_ptr<autofill::AutofillProfile> address) override;
  void SetContactInfo(
      std::unique_ptr<autofill::AutofillProfile> profile) override;
  void SetCreditCard(
      std::unique_ptr<autofill::CreditCard> card,
      std::unique_ptr<autofill::AutofillProfile> billing_profile) override;
  void SetTermsAndConditions(
      TermsAndConditionsState terms_and_conditions) override;
  void SetLoginOption(std::string identifier) override;
  void OnTextLinkClicked(int link) override;
  void OnFormActionLinkClicked(int link) override;
  void SetDateTimeRangeStartDate(
      const base::Optional<DateProto>& date) override;
  void SetDateTimeRangeStartTimeSlot(
      const base::Optional<int>& timeslot_index) override;
  void SetDateTimeRangeEndDate(const base::Optional<DateProto>& date) override;
  void SetDateTimeRangeEndTimeSlot(
      const base::Optional<int>& timeslot_index) override;
  void SetAdditionalValue(const std::string& client_memory_key,
                          const ValueProto& value) override;
  void GetTouchableArea(std::vector<RectF>* area) const override;
  void GetRestrictedArea(std::vector<RectF>* area) const override;
  void GetVisualViewport(RectF* visual_viewport) const override;
  void OnFatalError(const std::string& error_message,
                    Metrics::DropOutReason reason) override;
  void PerformDelayedShutdownIfNecessary();
  void MaybeReportFirstCheckDone();
  ViewportMode GetViewportMode() override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  void GetOverlayColors(OverlayColors* colors) const override;
  const ClientSettings& GetClientSettings() const override;
  const FormProto* GetForm() const override;
  const FormProto::Result* GetFormResult() const override;
  void SetCounterValue(int input_index, int counter_index, int value) override;
  void SetChoiceSelected(int input_index,
                         int choice_index,
                         bool selected) override;
  void AddObserver(ControllerObserver* observer) override;
  void RemoveObserver(const ControllerObserver* observer) override;
  void DispatchEvent(const EventHandler::EventKey& key) override;
  UserModel* GetUserModel() override;
  EventHandler* GetEventHandler() override;
  bool ShouldPromptActionExpandSheet() const override;
  BasicInteractions* GetBasicInteractions() override;
  const GenericUserInterfaceProto* GetGenericUiProto() const override;

 private:
  friend ControllerTest;

  void SetWebControllerForTest(std::unique_ptr<WebController> web_controller);

  // Called when the committed URL has or might have changed.
  void OnUrlChange();

  // Returns true if the controller should keep checking for scripts according
  // to the current state.
  bool ShouldCheckScripts();

  // If the controller's state requires scripts to be checked, check them
  // once right now and schedule regular checks. Otherwise, do nothing.
  void GetOrCheckScripts();

  void OnGetScripts(const GURL& url, bool result, const std::string& response);

  // Execute |script_path| and, if execution succeeds, enter |end_state| and
  // call |on_success|.
  void ExecuteScript(const std::string& script_path,
                     const std::string& start_message,
                     bool needs_ui,
                     std::unique_ptr<TriggerContext> context,
                     AutofillAssistantState end_state);
  void OnScriptExecuted(const std::string& script_path,
                        AutofillAssistantState end_state,
                        const ScriptExecutor::Result& result);

  // Check script preconditions every few seconds for a certain number of times.
  // If checks are already running, StartPeriodicScriptChecks resets the count.
  //
  // TODO(crbug.com/806868): Find a better solution. This is a brute-force
  // solution that reacts slowly to changes.
  void StartPeriodicScriptChecks();
  void StopPeriodicScriptChecks();
  void OnPeriodicScriptCheck();

  // Runs autostart scripts from |runnable_scripts|, if the conditions are
  // right. Returns true if a script was auto-started.
  bool MaybeAutostartScript(const std::vector<ScriptHandle>& runnable_scripts);

  void DisableAutostart();

  void InitFromParameters();

  // Called when a script is selected.
  void OnScriptSelected(const ScriptHandle& handle,
                        std::unique_ptr<TriggerContext> context);

  void UpdateCollectUserDataActions();
  void OnCollectUserDataContinueButtonClicked();
  void OnCollectUserDataAdditionalActionTriggered(int index);

  // Overrides ScriptTracker::Listener:
  void OnNoRunnableScriptsForPage() override;
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override;

  // Overrides content::WebContentsObserver:
  void DidAttachInterstitialPage() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

  // Overrides autofill_assistant::UserModel::Observer:
  void OnValueChanged(const std::string& identifier,
                      const ValueProto& new_value) override;

  void OnTouchableAreaChanged(const RectF& visual_viewport,
                              const std::vector<RectF>& touchable_areas,
                              const std::vector<RectF>& restricted_areas);

  void SetOverlayColors(std::unique_ptr<OverlayColors> colors);
  void ReportNavigationStateChanged();
  void SetProfile(const std::string& key,
                  UserData::FieldChange field_change,
                  std::unique_ptr<autofill::AutofillProfile> profile);

  // Show the first "Opening..." message and enter START state.
  void ShowFirstMessageAndStart();

  // Clear out visible state and enter the stopped state.
  void EnterStoppedState();

  ElementArea* touchable_element_area();
  ScriptTracker* script_tracker();
  bool allow_autostart() { return state_ == AutofillAssistantState::STARTING; }

  ClientSettings settings_;
  Client* const client_;
  const base::TickClock* const tick_clock_;

  // Lazily instantiate in GetWebController().
  std::unique_ptr<WebController> web_controller_;

  // Lazily instantiate in GetService().
  std::unique_ptr<Service> service_;
  std::unique_ptr<TriggerContext> trigger_context_;

  AutofillAssistantState state_ = AutofillAssistantState::INACTIVE;

  // The URL passed to Start(). Used only as long as there's no committed URL.
  // Note that this is the deeplink passed by a caller.
  GURL deeplink_url_;

  // The last URL the controller requested scripts from. Note that this is
  // reported to the backend in an initial get action request.
  GURL script_url_;

  // Whether a task for periodic checks is scheduled.
  bool periodic_script_check_scheduled_ = false;

  // Number of remaining periodic checks.
  int periodic_script_check_count_ = 0;

  // Run this script if no scripts become autostartable after
  // absolute_autostart_timeout.
  //
  // Ignored unless |allow_autostart_| is true.
  std::string autostart_timeout_script_path_;

  // How long to wait for an autostartable script before failing.
  base::TimeDelta autostart_timeout_;

  // Ticks at which we'll have reached |autostart_timeout_|.
  base::TimeTicks absolute_autostart_timeout_;

  // Area of the screen that corresponds to the current set of touchable
  // elements.
  // Lazily instantiate in touchable_element_area().
  std::unique_ptr<ElementArea> touchable_element_area_;

  // Current status message, may be empty.
  std::string status_message_;

  // Current bubble / tooltip message, may be empty.
  std::string bubble_message_;

  // Current details, may be null.
  std::unique_ptr<Details> details_;

  // Current info box, may be null.
  std::unique_ptr<InfoBox> info_box_;

  // Current progress.
  int progress_ = 0;

  // Current visibility of the progress bar. It is initially visible.
  bool progress_visible_ = true;

  // Current set of user actions. May be null, but never empty.
  std::unique_ptr<std::vector<UserAction>> user_actions_;

  // Current viewport mode.
  ViewportMode viewport_mode_ = ViewportMode::NO_RESIZE;

  // Current peek mode.
  ConfigureBottomSheetProto::PeekMode peek_mode_ =
      ConfigureBottomSheetProto::HANDLE;
  bool auto_change_peek_mode_ = false;

  std::unique_ptr<OverlayColors> overlay_colors_;

  CollectUserDataOptions* collect_user_data_options_ = nullptr;
  std::unique_ptr<UserData> user_data_;

  std::unique_ptr<FormProto> form_;
  std::unique_ptr<FormProto::Result> form_result_;
  base::RepeatingCallback<void(const FormProto::Result*)>
      form_changed_callback_ = base::DoNothing();
  base::OnceCallback<void(const ClientStatus&)> form_cancel_callback_ =
      base::DoNothing();

  // Value for ScriptExecutorDelegate::IsNavigatingToNewDocument()
  bool navigating_to_new_document_ = false;

  // If this is set, the controller was still navigating during startup. Wait
  // for DidFinishNavigation and execute the callback.
  base::OnceClosure start_after_navigation_;

  // Value for ScriptExecutorDelegate::HasNavigationError()
  bool navigation_error_ = false;
  base::ObserverList<NavigationListener> navigation_listeners_;

  // Tracks scripts and script execution. It's kept at the end, as it tend to
  // depend on everything the controller support, through script and script
  // actions.
  // Lazily instantiate in script_tracker().
  std::unique_ptr<ScriptTracker> script_tracker_;

  base::ObserverList<ControllerObserver> observers_;

  // If true, the controller is supposed to stay up and running in the
  // background even without UI, keeping track of scripts.
  //
  // This has two main effects:
  // - the controllers stays alive even after a fatal error, just so it can
  // immediately report that no actions are available on that website.
  // - scripts error are not considered fatal errors. The controller reverts
  // to TRACKING mode.
  //
  // This is set by Track().
  bool tracking_ = false;

  // Whether the controller is in a state in which a UI should be shown.
  bool needs_ui_ = false;

  // True once the controller has run the first set of scripts and have either
  // declared it invalid - and entered stopped state - or have processed its
  // result - and updated the state and set of available actions.
  bool has_run_first_check_ = false;

  // Callbacks to call when |has_run_first_check_| becomes true.
  std::vector<base::OnceCallback<void()>> on_has_run_first_check_;

  // If set, the controller entered the STOPPED state but shutdown was delayed
  // until the browser has left the |script_url_.host()| for which the decision
  // was taken.
  base::Optional<Metrics::DropOutReason> delayed_shutdown_reason_;

  EventHandler event_handler_;
  UserModel user_model_;
  BasicInteractions basic_interactions_{this};

  bool expand_sheet_for_prompt_action_ = true;
  std::vector<std::string> browse_domains_whitelist_;

  // Only set during a ShowGenericUiAction.
  std::unique_ptr<GenericUserInterfaceProto> generic_user_interface_;

  base::WeakPtrFactory<Controller> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_

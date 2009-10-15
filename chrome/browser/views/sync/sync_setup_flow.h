// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#ifndef CHROME_BROWSER_VIEWS_SYNC_SYNC_SETUP_FLOW_H_
#define CHROME_BROWSER_VIEWS_SYNC_SYNC_SETUP_FLOW_H_

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/time.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/views/sync/sync_setup_wizard.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class FlowHandler;
class SyncSetupFlowContainer;

// The state machine used by SyncSetupWizard, exposed in its own header
// to facilitate testing of SyncSetupWizard.  This class is used to open and
// run the html dialog and deletes itself when the dialog closes.
class SyncSetupFlow : public HtmlDialogUIDelegate {
 public:
  virtual ~SyncSetupFlow();

  // Runs a flow from |start| to |end|, and does the work of actually showing
  // the HTML dialog.  |container| is kept up-to-date with the lifetime of the
  // flow (e.g it is emptied on dialog close).
  static SyncSetupFlow* Run(ProfileSyncService* service,
                            SyncSetupFlowContainer* container,
                            SyncSetupWizard::State start,
                            SyncSetupWizard::State end);

  // Fills |args| with "user" and "error" arguments by querying |service|.
  static void GetArgsForGaiaLogin(
      const ProfileSyncService* service,
      DictionaryValue* args);

  // Triggers a state machine transition to advance_state.
  void Advance(SyncSetupWizard::State advance_state);

  // HtmlDialogUIDelegate implementation.
  // Get the HTML file path for the content to load in the dialog.
  virtual GURL GetDialogContentURL() const {
    return GURL("chrome://syncresources/setup");
  }

  // HtmlDialogUIDelegate implementation.
  virtual void GetDOMMessageHandlers(
      std::vector<DOMMessageHandler*>* handlers) const;

  // HtmlDialogUIDelegate implementation.
  // Get the size of the dialog.
  virtual void GetDialogSize(gfx::Size* size) const;

  // HtmlDialogUIDelegate implementation.
  // Gets the JSON string input to use when opening the dialog.
  virtual std::string GetDialogArgs() const {
    return dialog_start_args_;
  }

  // HtmlDialogUIDelegate implementation.
  // A callback to notify the delegate that the dialog closed.
  virtual void OnDialogClosed(const std::string& json_retval);

  // HtmlDialogUIDelegate implementation.
  virtual std::wstring GetDialogTitle() const {
    return l10n_util::GetString(IDS_SYNC_MY_BOOKMARKS_LABEL);
  }

  // HtmlDialogUIDelegate implementation.
  virtual bool IsDialogModal() const {
    return false;
  }

  void OnUserSubmittedAuth(const std::string& username,
                           const std::string& password) {
    service_->OnUserSubmittedAuth(username, password);
  }

  void OnUserAcceptedMergeAndSync() {
    service_->OnUserAcceptedMergeAndSync();
  }

 private:
  FRIEND_TEST(SyncSetupWizardTest, InitialStepLogin);
  FRIEND_TEST(SyncSetupWizardTest, InitialStepMergeAndSync);
  FRIEND_TEST(SyncSetupWizardTest, DialogCancelled);
  FRIEND_TEST(SyncSetupWizardTest, InvalidTransitions);
  FRIEND_TEST(SyncSetupWizardTest, FullSuccessfulRunSetsPref);
  FRIEND_TEST(SyncSetupWizardTest, DiscreteRun);

  // Use static Run method to get an instance.
  SyncSetupFlow(SyncSetupWizard::State start_state,
                SyncSetupWizard::State end_state,
                const std::string& args, SyncSetupFlowContainer* container,
                FlowHandler* handler, ProfileSyncService* service)
      : container_(container),
        dialog_start_args_(args),
        current_state_(start_state),
        end_state_(end_state),
        login_start_time_(base::TimeTicks::Now()),
        flow_handler_(handler),
        service_(service) {
  }

  // Returns true if |this| should transition its state machine to |state|
  // based on |current_state_|, or false if that would be nonsense or is
  // a no-op.
  bool ShouldAdvance(SyncSetupWizard::State state);

  SyncSetupFlowContainer* container_;  // Our container.  Don't own this.
  std::string dialog_start_args_;  // The args to pass to the initial page.

  SyncSetupWizard::State current_state_;
  SyncSetupWizard::State end_state_;  // The goal.

  // Time that the GAIA_LOGIN step was received.
  base::TimeTicks login_start_time_;

  // The handler needed for the entire flow.  We don't own this.
  FlowHandler* flow_handler_;

  // We need this to write the sentinel "setup completed" pref.
  ProfileSyncService* service_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupFlow);
};

// A really simple wrapper for a SyncSetupFlow so that we don't have to
// add any public methods to the public SyncSetupWizard interface to notify it
// when the dialog closes.
class SyncSetupFlowContainer {
 public:
  SyncSetupFlowContainer() : flow_(NULL) { }
  void set_flow(SyncSetupFlow* flow) {
    DCHECK(!flow_ || !flow);
    flow_ = flow;
  }

  SyncSetupFlow* get_flow() { return flow_; }
 private:
  SyncSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupFlowContainer);
};

// The FlowHandler connects the state machine to the dialog backing HTML and
// JS namespace by implementing DOMMessageHandler and being invoked by the
// SyncSetupFlow.  Exposed here to facilitate testing.
class FlowHandler : public DOMMessageHandler {
 public:
  FlowHandler()  {}
  virtual ~FlowHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callbacks from the page.
  void HandleSubmitAuth(const Value* value);
  void HandleSubmitMergeAndSync(const Value* value);

  // These functions control which part of the HTML is visible.
  void ShowGaiaLogin(const DictionaryValue& args);
  void ShowGaiaSuccessAndClose();
  void ShowGaiaSuccessAndSettingUp();
  void ShowMergeAndSync();
  void ShowMergeAndSyncError();
  void ShowSetupDone(const std::wstring& user);
  void ShowFirstTimeDone(const std::wstring& user);

  void set_flow(SyncSetupFlow* flow) {
    flow_ = flow;
  }

 private:
  void ExecuteJavascriptInIFrame(const std::wstring& iframe_xpath,
                                 const std::wstring& js);
  SyncSetupFlow* flow_;
  DISALLOW_COPY_AND_ASSIGN(FlowHandler);
};

#endif  // CHROME_BROWSER_VIEWS_SYNC_SYNC_SETUP_FLOW_H_
#endif  // CHROME_PERSONALIZATION

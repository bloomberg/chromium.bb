// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_SETUP_FLOW_H_
#define CHROME_BROWSER_REMOTING_SETUP_FLOW_H_

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"

class ListValue;

namespace remoting {

class SetupFlow;

// SetupFlowStep represents a single step for SetupFlow, e.g. login or
// host registration. When a step is finished, GetNextStep() is called
// to get the step that must follow.
class SetupFlowStep {
 public:
  typedef Callback0::Type DoneCallback;

  SetupFlowStep();
  virtual ~SetupFlowStep();

  // Start the step. Ownership of |done_callback| is given to the
  // function. |done_callback| is called when the step is finished,
  // The callback must be called on the same thread as Start().
  virtual void Start(SetupFlow* flow, DoneCallback* done_callback) = 0;

  // Called to handle |message| received from UI.
  virtual void HandleMessage(const std::string& message,
                             const ListValue* args) = 0;

  // Called if user closes the dialog.
  virtual void Cancel() = 0;

  // Returns SetupFlowStep object that corresponds to the next
  // step. Must never return NULL.
  virtual SetupFlowStep* GetNextStep() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SetupFlowStep);
};

// SetupFlowStepBase implements base functions common for all
// SetupFlowStep implementations.
class SetupFlowStepBase : public SetupFlowStep {
 public:
  SetupFlowStepBase();
  ~SetupFlowStepBase();

  // SetupFlowStep implementation.
  virtual void Start(SetupFlow* flow, DoneCallback* done_callback);
  virtual SetupFlowStep* GetNextStep();

 protected:
  SetupFlow* flow() { return flow_; }

  void ExecuteJavascriptInIFrame(const std::wstring& iframe_xpath,
                                 const std::wstring& js);

  // Finish current step. Calls |done_callback| specified in Start().
  // GetNextStep() will return the specified |next_step|.
  void FinishStep(SetupFlowStep* next_step);

  // Called from Start(). Child classes must override this method
  // instead of Start().
  virtual void DoStart() = 0;

 private:
  SetupFlow* flow_;
  scoped_ptr<DoneCallback> done_callback_;
  bool done_;

  // Next step stored between Done() and GetNextStep();
  SetupFlowStep* next_step_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlowStepBase);
};

// The last step in the setup flow. This step never finishes, user is
// expected to close dialog after that.
class SetupFlowDoneStep : public SetupFlowStepBase {
 public:
  SetupFlowDoneStep();
  virtual ~SetupFlowDoneStep();

  // SetupFlowStep implementation.
  virtual void HandleMessage(const std::string& message, const ListValue* args);
  virtual void Cancel();

 protected:
  void DoStart();

 private:
  DISALLOW_COPY_AND_ASSIGN(SetupFlowDoneStep);
};

// This class is responsible for showing a remoting setup dialog and
// perform operations to fill the content of the dialog and handle
// user actions in the dialog.
class SetupFlow : public DOMMessageHandler,
                  public HtmlDialogUIDelegate {
 public:
  virtual ~SetupFlow();

  static SetupFlow* OpenSetupDialog(Profile* profile);

  DOMUI* dom_ui() { return dom_ui_; }
  Profile* profile() { return profile_; }

 private:
  explicit SetupFlow(const std::string& args, Profile* profile,
                     SetupFlowStep* first_step);

  // HtmlDialogUIDelegate implementation.
  virtual GURL GetDialogContentURL() const;
  virtual void GetDOMMessageHandlers(
      std::vector<DOMMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog);
  virtual std::wstring GetDialogTitle() const;
  virtual bool IsDialogModal() const;
  virtual bool ShouldShowDialogTitle() const;

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  void HandleSubmitAuth(const ListValue* args);

  void StartCurrentStep();
  void OnStepDone();

  // Pointer to the DOM UI. This is provided by RemotingSetupMessageHandler
  // when attached.
  DOMUI* dom_ui_;

  // The args to pass to the initial page.
  std::string dialog_start_args_;
  Profile* profile_;

  scoped_ptr<SetupFlowStep> current_step_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlow);
};

}  // namespace remoting

#endif  // CHROME_BROWSER_REMOTING_SETUP_FLOW_H_

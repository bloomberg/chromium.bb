// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"
#include "components/autofill/content/browser/request_autocomplete_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace IPC {
class Message;
}

namespace autofill {

class AutofillContext;
class AutofillClient;

// Class that drives autofill flow in the browser process based on
// communication from the renderer and from the external world. There is one
// instance per WebContents.
class ContentAutofillDriver : public AutofillDriver,
                              public content::WebContentsObserver,
                              public base::SupportsUserData::Data {
 public:
  static void CreateForWebContentsAndDelegate(
      content::WebContents* contents,
      AutofillClient* client,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);
  static ContentAutofillDriver* FromWebContents(content::WebContents* contents);

  // AutofillDriver:
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual net::URLRequestContextGetter* GetURLRequestContext() OVERRIDE;
  virtual base::SequencedWorkerPool* GetBlockingPool() OVERRIDE;
  virtual bool RendererIsAvailable() OVERRIDE;
  virtual void SendFormDataToRenderer(int query_id,
                                      RendererFormDataAction action,
                                      const FormData& data) OVERRIDE;
  virtual void PingRenderer() OVERRIDE;
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) OVERRIDE;
  virtual void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) OVERRIDE;
  virtual void RendererShouldClearFilledForm() OVERRIDE;
  virtual void RendererShouldClearPreviewedForm() OVERRIDE;
  virtual void RendererShouldFillFieldWithValue(
      const base::string16& value) OVERRIDE;
  virtual void RendererShouldPreviewFieldWithValue(
      const base::string16& value) OVERRIDE;

  // Returns the WebContents with which this instance is associated.
  content::WebContents* GetWebContents();

  AutofillExternalDelegate* autofill_external_delegate() {
    return &autofill_external_delegate_;
  }

  AutofillManager* autofill_manager() { return autofill_manager_.get(); }

 protected:
  ContentAutofillDriver(
      content::WebContents* web_contents,
      AutofillClient* client,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);
  virtual ~ContentAutofillDriver();

  // content::WebContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Sets the manager to |manager| and sets |manager|'s external delegate
  // to |autofill_external_delegate_|. Takes ownership of |manager|.
  void SetAutofillManager(scoped_ptr<AutofillManager> manager);

 private:
  // AutofillManager instance via which this object drives the shared Autofill
  // code.
  scoped_ptr<AutofillManager> autofill_manager_;

  // AutofillExternalDelegate instance that this object instantiates in the
  // case where the Autofill native UI is enabled.
  AutofillExternalDelegate autofill_external_delegate_;

  // Driver for the interactive autocomplete dialog.
  RequestAutocompleteManager request_autocomplete_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_

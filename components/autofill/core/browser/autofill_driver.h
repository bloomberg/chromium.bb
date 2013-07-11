// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

#include <vector>

#include "components/autofill/core/common/form_data.h"

namespace content {
class WebContents;
}

namespace autofill {

class FormStructure;

// Interface that allows Autofill core code to interact with its driver (i.e.,
// obtain information from it and give information to it). A concrete
// implementation must be provided by the driver.
class AutofillDriver {
 public:
   // The possible actions that the renderer can take on receiving form data.
  enum RendererFormDataAction {
    // The renderer should fill the form data.
    FORM_DATA_ACTION_FILL,
    // The renderer should preview the form data.
    FORM_DATA_ACTION_PREVIEW
  };

  virtual ~AutofillDriver() {}

  // TODO(blundell): Remove this method once shared code no longer needs to
  // know about WebContents.
  virtual content::WebContents* GetWebContents() = 0;

  // Returns true iff the renderer is available for communication.
  virtual bool RendererIsAvailable() = 0;

  // Informs the renderer what action to take with the next form data that it
  // receives. Must be called before each call to |SendFormDataToRenderer|.
  virtual void SetRendererActionOnFormDataReception(
      RendererFormDataAction action) = 0;

  // Forwards |data| to the renderer. |query_id| is the id of the renderer's
  // original request for the data. This method is a no-op if the renderer is
  // not currently available.
  virtual void SendFormDataToRenderer(int query_id, const FormData& data) = 0;

  // Sends the field type predictions specified in |forms| to the renderer. This
  // method is a no-op if the renderer is not available or the appropriate
  // command-line flag is not set.
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) = 0;

  // Tells the renderer to clear the currently filled Autofill results.
  virtual void RendererShouldClearFilledForm() = 0;

  // Tells the renderer to clear the currently previewed Autofill results.
  virtual void RendererShouldClearPreviewedForm() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_input.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace autofill {
struct FormData;
struct FormFieldData;
}

namespace autofill_assistant {
// Controller to interact with the web pages.
class WebController {
 public:
  // Create web controller for a given |web_contents|.
  static std::unique_ptr<WebController> CreateForWebContents(
      content::WebContents* web_contents);

  // |web_contents| must outlive this web controller.
  WebController(content::WebContents* web_contents,
                std::unique_ptr<DevtoolsClient> devtools_client);
  virtual ~WebController();

  // Returns the last committed URL of the associated |web_contents_|.
  virtual const GURL& GetUrl();

  // Perform a mouse left button click on the element given by |selectors| and
  // return the result through callback.
  // CSS selectors in |selectors| are ordered from top frame to the frame
  // contains the element and the element.
  virtual void ClickElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback);

  // Check whether the element given by |selectors| exists on the web page.
  virtual void ElementExists(const std::vector<std::string>& selectors,
                             base::OnceCallback<void(bool)> callback);

  // Fill the address form given by |selectors| with the given address |guid| in
  // personal data manager.
  virtual void FillAddressForm(const std::string& guid,
                               const std::vector<std::string>& selectors,
                               base::OnceCallback<void(bool)> callback);

  // Fill the card form given by |selectors| with the given card |guid| in
  // personal data manager.
  virtual void FillCardForm(const std::string& guid,
                            const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback);

  // Select the option given by |selectors| and the value of the option to be
  // picked.
  virtual void SelectOption(const std::vector<std::string>& selectors,
                            const std::string& selected_option,
                            base::OnceCallback<void(bool)> callback);

  // Focus on element given by |selectors|.
  virtual void FocusElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback);

  // Get the value of all fields in |selector_list| and return the result
  // through |callback|. The list of returned values will have the same size as
  // |selectors_list|, and will be the empty string in case of error or empty
  // value.
  virtual void GetFieldsValue(
      const std::vector<std::vector<std::string>>& selectors_list,
      base::OnceCallback<void(const std::vector<std::string>&)> callback);

 private:
  friend class WebControllerBrowserTest;

  struct FindElementResult {
    FindElementResult() = default;
    ~FindElementResult() = default;

    // The render frame host contains the element.
    content::RenderFrameHost* container_frame_host;

    // The selector index in the given selectors corresponding to the container
    // frame. Zero indicates the element is in main frame or the first element
    // is the container frame selector. Compare main frame with the above
    // |container_frame_host| to distinguish them.
    size_t container_frame_selector_index;

    // The object id of the element.
    std::string object_id;
  };
  using FindElementCallback =
      base::OnceCallback<void(std::unique_ptr<FindElementResult>)>;

  void OnFindElementForClick(base::OnceCallback<void(bool)> callback,
                             std::unique_ptr<FindElementResult> result);
  void ClickObject(const std::string& object_id,
                   base::OnceCallback<void(bool)> callback);
  void OnScrollIntoView(base::OnceCallback<void(bool)> callback,
                        std::string object_id,
                        std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnGetBoxModel(base::OnceCallback<void(bool)> callback,
                     std::unique_ptr<dom::GetBoxModelResult> result);
  void OnDispatchPressMoustEvent(
      base::OnceCallback<void(bool)> callback,
      double x,
      double y,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchReleaseMoustEvent(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnFindElementForExist(base::OnceCallback<void(bool)> callback,
                             std::unique_ptr<FindElementResult> result);
  void FindElement(const std::vector<std::string>& selectors,
                   FindElementCallback callback);
  void OnGetDocument(const std::vector<std::string>& selectors,
                     FindElementCallback callback,
                     std::unique_ptr<dom::GetDocumentResult> result);
  void RecursiveFindElement(int node_id,
                            size_t index,
                            const std::vector<std::string>& selectors,
                            std::unique_ptr<FindElementResult> element_result,
                            FindElementCallback callback);
  void OnQuerySelectorAll(size_t index,
                          const std::vector<std::string>& selectors,
                          std::unique_ptr<FindElementResult> element_result,
                          FindElementCallback callback,
                          std::unique_ptr<dom::QuerySelectorAllResult> result);
  void OnResolveNode(std::unique_ptr<FindElementResult> element_result,
                     FindElementCallback callback,
                     std::unique_ptr<dom::ResolveNodeResult> result);
  void OnDescribeNode(int node_id,
                      size_t index,
                      const std::vector<std::string>& selectors,
                      std::unique_ptr<FindElementResult> element_result,
                      FindElementCallback callback,
                      std::unique_ptr<dom::DescribeNodeResult> result);
  content::RenderFrameHost* FindCorrespondingRenderFrameHost(
      std::string name,
      std::string document_url);
  void OnPushNodesByBackendIds(
      size_t index,
      const std::vector<std::string>& selectors,
      std::unique_ptr<FindElementResult> element_result,
      FindElementCallback callback,
      std::unique_ptr<dom::PushNodesByBackendIdsToFrontendResult> result);
  void OnResult(bool result, base::OnceCallback<void(bool)> callback);
  void OnFindElementForFillingForm(
      const std::string& autofill_data_guid,
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnClickObjectForFillingForm(
      const std::string& autofill_data_guid,
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result,
      bool click_result);
  void OnGetFormAndFieldDataForFillingForm(
      const std::string& autofill_data_guid,
      base::OnceCallback<void(bool)> callback,
      content::RenderFrameHost* container_frame_host,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  void OnFindElementForFocusElement(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnFocusElement(base::OnceCallback<void(bool)> callback,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);

  // Weak pointer is fine here since it must outlive this web controller, which
  // is guaranteed by the owner of this object.
  content::WebContents* web_contents_;
  std::unique_ptr<DevtoolsClient> devtools_client_;

  base::WeakPtrFactory<WebController> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebController);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CONTROLLER_H_

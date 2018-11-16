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
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_input.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/rectf.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

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
//
// WARNING: Accessing or modifying page elements must be run in sequence: wait
// until the result of the first operation has been given to the callback before
// starting a new operation.
//
// TODO(crbug.com/806868): Figure out the reason for this limitation and fix it.
// Also, consider structuring the WebController to make it easier to run
// multiple operations, whether in sequence or in parallel.
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

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url);

  // Perform a mouse left button click or a touch tap on the element given by
  // |selectors| and return the result through callback.
  // CSS selectors in |selectors| are ordered from top frame to the frame
  // contains the element and the element.
  virtual void ClickOrTapElement(const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback);

  // Fill the address form given by |selectors| with the given address
  // |profile|.
  virtual void FillAddressForm(const autofill::AutofillProfile* profile,
                               const std::vector<std::string>& selectors,
                               base::OnceCallback<void(bool)> callback);

  // Fill the card form given by |selectors| with the given |card| and its
  // |cvc|.
  virtual void FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                            const base::string16& cvc,
                            const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback);

  // Select the option given by |selectors| and the value of the option to be
  // picked.
  virtual void SelectOption(const std::vector<std::string>& selectors,
                            const std::string& selected_option,
                            base::OnceCallback<void(bool)> callback);

  // Highlight an element given by |selectors|.
  virtual void HighlightElement(const std::vector<std::string>& selectors,
                                base::OnceCallback<void(bool)> callback);

  // Focus on element given by |selectors|.
  virtual void FocusElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback);

  // Set the |value| of field |selectors| and return the result through
  // |callback|. If |simulate_key_presses| is true, the value will be set by
  // clicking the field and then simulating key presses, otherwise the `value`
  // attribute will be set directly.
  virtual void SetFieldValue(const std::vector<std::string>& selectors,
                             const std::string& value,
                             bool simulate_key_presses,
                             base::OnceCallback<void(bool)> callback);

  // Set the |value| of the |attribute| of the element given by |selectors|.
  virtual void SetAttribute(const std::vector<std::string>& selectors,
                            const std::vector<std::string>& attribute,
                            const std::string& value,
                            base::OnceCallback<void(bool)> callback);

  // Return the outerHTML of |selectors|.
  virtual void GetOuterHtml(
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool, const std::string&)> callback);

  // Create a helper for checking element existence and field value.
  virtual std::unique_ptr<BatchElementChecker> CreateBatchElementChecker();

  // Gets the position of the element identified by the selector.
  //
  // If unsuccessful, the callback gets (false, 0, 0, 0, 0).
  //
  // If successful, the callback gets (true, left, top, right, bottom), with
  // coordinates expressed as numbers between 0 and 1, relative to the width or
  // height of the visible viewport.
  virtual void GetElementPosition(
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool, const RectF&)> callback);

  // Scroll the view by the given distance.
  //
  // Distances are floats between -1.0 and 1.0, with 1 corresponding to the size
  // of the visible viewport.
  virtual void ScrollBy(float distanceXRatio, float distanceYRatio);

 protected:
  friend class BatchElementChecker;

  // Checks an element for:
  //
  // kExistenceCheck: Checks whether at least one element given by |selectors|
  // exists on the web page.
  //
  // kVisibilityCheck: Checks whether at least on element given by |selectors|
  // is visible on the web page.
  //
  // Normally done through BatchElementChecker.
  virtual void ElementCheck(ElementCheckType type,
                            const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback);

  // Get the value of |selectors| and return the result through |callback|. The
  // returned value might be false, if the element cannot be found, true and the
  // empty string in case of error or empty value.
  //
  // Normally done through BatchElementChecker.
  virtual void GetFieldValue(
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool, const std::string&)> callback);

 private:
  friend class WebControllerBrowserTest;

  // Helper class to get element's position when is stable and the frame it
  // belongs finished visual update.
  class ElementPositionGetter {
   public:
    ElementPositionGetter();
    ~ElementPositionGetter();

    // |devtools_client| must outlive this class which is guarantteed by the
    // owner of this class.
    void Start(content::RenderFrameHost* frame_host,
               DevtoolsClient* devtools_client,
               std::string element_object_id,
               base::OnceCallback<void(int, int)> callback);

   private:
    void OnVisualStateUpdatedCallback(bool state);
    void GetAndWaitBoxModelStable(DevtoolsClient* devtools_client,
                                  std::string object_id,
                                  int point_x,
                                  int point_y,
                                  int remaining_rounds);
    void OnGetBoxModelForStableCheck(
        DevtoolsClient* devtools_client,
        std::string object_id,
        int point_x,
        int point_y,
        int remaining_rounds,
        std::unique_ptr<dom::GetBoxModelResult> result);
    void OnResult(int x, int y);

    base::OnceCallback<void(int, int)> callback_;
    bool visual_state_updated_;

    base::WeakPtrFactory<ElementPositionGetter> weak_ptr_factory_;
    DISALLOW_COPY_AND_ASSIGN(ElementPositionGetter);
  };

  // Perform a mouse left button click on the element given by |selectors| and
  // return the result through callback.
  // CSS selectors in |selectors| are ordered from top frame to the frame
  // contains the element and the element.
  void ClickElement(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback);

  // Perform a touch tap on the element given by |selectors| and return the
  // result through callback. CSS selectors in |selectors| are ordered from top
  // frame to the frame contains the element and the element.
  void TapElement(const std::vector<std::string>& selectors,
                  base::OnceCallback<void(bool)> callback);

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

  struct FillFormInputData {
    FillFormInputData();
    ~FillFormInputData();

    // Data for filling address form.
    std::unique_ptr<autofill::AutofillProfile> profile;

    // Data for filling card form.
    std::unique_ptr<autofill::CreditCard> card;
    base::string16 cvc;
  };

  void OnFindElementForClickOrTap(base::OnceCallback<void(bool)> callback,
                                  bool is_a_click,
                                  std::unique_ptr<FindElementResult> result);
  void OnFindElementForTap(base::OnceCallback<void(bool)> callback,
                           std::unique_ptr<FindElementResult> result);
  void ClickOrTapElement(std::unique_ptr<FindElementResult> target_element,
                         bool is_a_click,
                         base::OnceCallback<void(bool)> callback);
  void OnScrollIntoView(std::unique_ptr<FindElementResult> target_element,
                        base::OnceCallback<void(bool)> callback,
                        bool is_a_click,
                        std::unique_ptr<runtime::CallFunctionOnResult> result);
  void TapOrClickOnCoordinates(
      std::unique_ptr<ElementPositionGetter> element_position_getter,
      base::OnceCallback<void(bool)> callback,
      bool is_a_click,
      int x,
      int y);
  void OnDispatchPressMouseEvent(
      base::OnceCallback<void(bool)> callback,
      int x,
      int y,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchReleaseMouseEvent(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchTouchEventStart(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<input::DispatchTouchEventResult> result);
  void OnDispatchTouchEventEnd(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<input::DispatchTouchEventResult> result);
  void OnFindElementForCheck(ElementCheckType check_type,
                             base::OnceCallback<void(bool)> callback,
                             std::unique_ptr<FindElementResult> result);
  void OnGetBoxModelForVisible(base::OnceCallback<void(bool)> callback,
                               std::unique_ptr<dom::GetBoxModelResult> result);

  // Find the element given by |selectors|. If multiple elements match
  // |selectors| and if |strict_mode| is false, return the first one that is
  // found. Otherwise if |strict-mode| is true, do not return any.
  void FindElement(const std::vector<std::string>& selectors,
                   bool strict_mode,
                   FindElementCallback callback);
  void OnGetDocumentElement(const std::vector<std::string>& selectors,
                            bool strict_mode,
                            FindElementCallback callback,
                            std::unique_ptr<runtime::EvaluateResult> result);
  void RecursiveFindElement(const std::string& object_id,
                            size_t index,
                            const std::vector<std::string>& selectors,
                            bool strict_mode,
                            std::unique_ptr<FindElementResult> element_result,
                            FindElementCallback callback);
  void OnQuerySelectorAll(
      size_t index,
      const std::vector<std::string>& selectors,
      bool strict_mode,
      std::unique_ptr<FindElementResult> element_result,
      FindElementCallback callback,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnDescribeNode(const std::string& object_id,
                      size_t index,
                      const std::vector<std::string>& selectors,
                      bool strict_mode,
                      std::unique_ptr<FindElementResult> element_result,
                      FindElementCallback callback,
                      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNode(size_t index,
                     const std::vector<std::string>& selectors,
                     bool strict_mode,
                     std::unique_ptr<FindElementResult> element_result,
                     FindElementCallback callback,
                     std::unique_ptr<dom::ResolveNodeResult> result);
  content::RenderFrameHost* FindCorrespondingRenderFrameHost(
      std::string name,
      std::string document_url);
  void OnResult(bool result, base::OnceCallback<void(bool)> callback);
  void OnResult(bool exists,
                const std::string& result,
                base::OnceCallback<void(bool, const std::string&)> callback);
  void OnFindElementForFillingForm(
      std::unique_ptr<FillFormInputData> data_to_autofill,
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnGetFormAndFieldDataForFillingForm(
      std::unique_ptr<FillFormInputData> data_to_autofill,
      base::OnceCallback<void(bool)> callback,
      content::RenderFrameHost* container_frame_host,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  void OnFindElementForFocusElement(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnFocusElement(base::OnceCallback<void(bool)> callback,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForSelectOption(
      const std::string& selected_option,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnSelectOption(base::OnceCallback<void(bool)> callback,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForHighlightElement(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnHighlightElement(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForGetFieldValue(
      base::OnceCallback<void(bool, const std::string&)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnGetValueAttribute(
      base::OnceCallback<void(bool, const std::string&)> callback,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void InternalSetFieldValue(const std::vector<std::string>& selectors,
                             const std::string& value,
                             base::OnceCallback<void(bool)> callback);
  void OnClearFieldForDispatchKeyEvent(
      const std::vector<std::string>& selectors,
      const std::string& value,
      base::OnceCallback<void(bool)> callback,
      bool clear_status);
  void OnClickElementForDispatchKeyEvent(
      const std::string& value,
      base::OnceCallback<void(bool)> callback,
      bool click_status);
  void DispatchKeyDownEvent(const std::string& value,
                            size_t index,
                            base::OnceCallback<void(bool)> callback);
  void DispatchKeyUpEvent(const std::string& value,
                          size_t index,
                          base::OnceCallback<void(bool)> callback);
  void OnDispatchKeyUpEvent(const std::string& value,
                            size_t index,
                            base::OnceCallback<void(bool)> callback);
  void OnFindElementForSetAttribute(
      const std::vector<std::string>& attribute,
      const std::string& value,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnSetAttribute(base::OnceCallback<void(bool)> callback,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForSetFieldValue(
      const std::string& value,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnSetValueAttribute(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForGetOuterHtml(
      base::OnceCallback<void(bool, const std::string&)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnGetOuterHtml(
      base::OnceCallback<void(bool, const std::string&)> callback,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  void OnFindElementForPosition(
      base::OnceCallback<void(bool, const RectF&)> callback,
      std::unique_ptr<FindElementResult> result);

  void OnGetElementPositionResult(
      base::OnceCallback<void(bool, const RectF&)> callback,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  // Weak pointer is fine here since it must outlive this web controller, which
  // is guaranteed by the owner of this object.
  content::WebContents* web_contents_;
  std::unique_ptr<DevtoolsClient> devtools_client_;

  base::WeakPtrFactory<WebController> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebController);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CONTROLLER_H_

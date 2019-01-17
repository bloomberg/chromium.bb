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
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_network.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/selector.h"

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
  // |selector| and return the result through callback.
  virtual void ClickOrTapElement(const Selector& selector,
                                 base::OnceCallback<void(bool)> callback);

  // Fill the address form given by |selector| with the given address
  // |profile|.
  virtual void FillAddressForm(const autofill::AutofillProfile* profile,
                               const Selector& selector,
                               base::OnceCallback<void(bool)> callback);

  // Fill the card form given by |selector| with the given |card| and its
  // |cvc|.
  virtual void FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                            const base::string16& cvc,
                            const Selector& selector,
                            base::OnceCallback<void(bool)> callback);

  // Select the option given by |selector| and the value of the option to be
  // picked.
  virtual void SelectOption(const Selector& selector,
                            const std::string& selected_option,
                            base::OnceCallback<void(bool)> callback);

  // Highlight an element given by |selector|.
  virtual void HighlightElement(const Selector& selector,
                                base::OnceCallback<void(bool)> callback);

  // Focus on element given by |selector|.
  virtual void FocusElement(const Selector& selector,
                            base::OnceCallback<void(bool)> callback);

  // Set the |value| of field |selector| and return the result through
  // |callback|. If |simulate_key_presses| is true, the value will be set by
  // clicking the field and then simulating key presses, otherwise the `value`
  // attribute will be set directly.
  virtual void SetFieldValue(const Selector& selector,
                             const std::string& value,
                             bool simulate_key_presses,
                             base::OnceCallback<void(bool)> callback);

  // Set the |value| of the |attribute| of the element given by |selector|.
  virtual void SetAttribute(const Selector& selector,
                            const std::vector<std::string>& attribute,
                            const std::string& value,
                            base::OnceCallback<void(bool)> callback);

  // Sets the keyboard focus to |selector| and inputs the specified UTF-8
  // characters in the specified order.
  // Returns the result through |callback|.
  virtual void SendKeyboardInput(const Selector& selector,
                                 const std::vector<std::string>& utf8_chars,
                                 base::OnceCallback<void(bool)> callback);

  // Return the outerHTML of |selector|.
  virtual void GetOuterHtml(
      const Selector& selector,
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
      const Selector& selector,
      base::OnceCallback<void(bool, const RectF&)> callback);

  // Functions to set, get and expire the Autofill Assistant cookie used to
  // detect when Autofill Assistant has been used on a domain before.
  virtual void SetCookie(const std::string& domain,
                         base::OnceCallback<void(bool)> callback);
  virtual void HasCookie(base::OnceCallback<void(bool)> callback);
  virtual void ClearCookie();

 protected:
  friend class BatchElementChecker;

  // Checks an element for:
  //
  // kExistenceCheck: Checks whether at least one element given by |selector|
  // exists on the web page.
  //
  // kVisibilityCheck: Checks whether at least on element given by |selector|
  // is visible on the web page.
  //
  // Normally done through BatchElementChecker.
  virtual void ElementCheck(ElementCheckType type,
                            const Selector& selector,
                            base::OnceCallback<void(bool)> callback);

  // Get the value of |selector| and return the result through |callback|. The
  // returned value might be false, if the element cannot be found, true and the
  // empty string in case of error or empty value.
  //
  // Normally done through BatchElementChecker.
  virtual void GetFieldValue(
      const Selector& selector,
      base::OnceCallback<void(bool, const std::string&)> callback);

 private:
  friend class WebControllerBrowserTest;

  // Callback that receives the position that corresponds to the center
  // of an element, from ElementPositionGetter.
  //
  // If the first element is false, the call failed. Otherwise, the second
  // element contains the x position and the third the y position of the center
  // of the element in viewport coordinates.
  using ElementPositionCallback = base::OnceCallback<void(bool, int, int)>;

  // Helper class to get element's position in viewport coordinates when is
  // stable and the frame it belongs finished visual update.
  class ElementPositionGetter {
   public:
    ElementPositionGetter();
    ~ElementPositionGetter();

    // |devtools_client| must outlive this class which is guarantteed by the
    // owner of this class.
    void Start(content::RenderFrameHost* frame_host,
               DevtoolsClient* devtools_client,
               std::string element_object_id,
               ElementPositionCallback callback);

   private:
    void OnVisualStateUpdatedCallback(bool success);
    void GetAndWaitBoxModelStable();
    void OnGetBoxModelForStableCheck(
        std::unique_ptr<dom::GetBoxModelResult> result);
    void OnScrollIntoView(
        std::unique_ptr<runtime::CallFunctionOnResult> result);
    void OnResult(int x, int y);
    void OnError();

    DevtoolsClient* devtools_client_ = nullptr;
    std::string object_id_;
    int remaining_rounds_ = 0;
    ElementPositionCallback callback_;
    bool visual_state_updated_ = false;

    // If |has_point_| is true, |point_x_| and |point_y_| contain the last
    // computed center of the element, in viewport coordinates. Note that
    // negative coordinates are valid, in case the element is above or to the
    // left of the viewport.
    bool has_point_ = false;
    int point_x_ = 0;
    int point_y_ = 0;

    base::WeakPtrFactory<ElementPositionGetter> weak_ptr_factory_;
    DISALLOW_COPY_AND_ASSIGN(ElementPositionGetter);
  };

  // Perform a mouse left button click on the element given by |selector| and
  // return the result through callback.
  void ClickElement(const Selector& selector,
                    base::OnceCallback<void(bool)> callback);

  // Perform a touch tap on the element given by |selector| and return the
  // result through callback.
  void TapElement(const Selector& selector,
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
      bool has_coordinates,
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

  // Find the element given by |selector|. If multiple elements match
  // |selector| and if |strict_mode| is false, return the first one that is
  // found. Otherwise if |strict-mode| is true, do not return any.
  void FindElement(const Selector& selector,
                   bool strict_mode,
                   FindElementCallback callback);
  void OnGetDocumentElement(const Selector& selector,
                            bool strict_mode,
                            FindElementCallback callback,
                            std::unique_ptr<runtime::EvaluateResult> result);
  void RecursiveFindElement(const std::string& object_id,
                            size_t index,
                            const Selector& selector,
                            bool strict_mode,
                            std::unique_ptr<FindElementResult> element_result,
                            FindElementCallback callback);
  void OnQuerySelectorAll(
      size_t index,
      const Selector& selector,
      bool strict_mode,
      std::unique_ptr<FindElementResult> element_result,
      FindElementCallback callback,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnDescribeNodeForPseudoElement(
      dom::PseudoType pseudo_type,
      std::unique_ptr<FindElementResult> element_result,
      FindElementCallback callback,
      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNodeForPseudoElement(
      std::unique_ptr<FindElementResult> element_result,
      FindElementCallback callback,
      std::unique_ptr<dom::ResolveNodeResult> result);
  void OnDescribeNode(const std::string& object_id,
                      size_t index,
                      const Selector& selector,
                      bool strict_mode,
                      std::unique_ptr<FindElementResult> element_result,
                      FindElementCallback callback,
                      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNode(size_t index,
                     const Selector& selector,
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
      const Selector& selector,
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
  void InternalSetFieldValue(const Selector& selector,
                             const std::string& value,
                             base::OnceCallback<void(bool)> callback);
  void OnClearFieldForSendKeyboardInput(
      const Selector& selector,
      const std::vector<std::string>& utf8_chars,
      base::OnceCallback<void(bool)> callback,
      bool clear_status);
  void OnClickElementForSendKeyboardInput(
      const std::vector<std::string>& utf8_chars,
      base::OnceCallback<void(bool)> callback,
      bool click_status);
  void DispatchKeyboardTextDownEvent(const std::vector<std::string>& utf8_chars,
                                     size_t index,
                                     base::OnceCallback<void(bool)> callback);
  void DispatchKeyboardTextUpEvent(const std::vector<std::string>& utf8_chars,
                                   size_t index,
                                   base::OnceCallback<void(bool)> callback);
  void OnFindElementForSetAttribute(
      const std::vector<std::string>& attribute,
      const std::string& value,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnSetAttribute(base::OnceCallback<void(bool)> callback,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForSendKeyboardInput(
      const Selector& selector,
      const std::vector<std::string>& utf8_chars,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<FindElementResult> element_result);
  void OnPressKeyboard(int key_code,
                       base::OnceCallback<void(bool)> callback,
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

  // Creates a new instance of DispatchKeyEventParams for the specified type and
  // text.
  using DispatchKeyEventParamsPtr =
      std::unique_ptr<autofill_assistant::input::DispatchKeyEventParams>;
  static DispatchKeyEventParamsPtr CreateKeyEventParamsFromText(
      autofill_assistant::input::DispatchKeyEventType type,
      const std::string& text);

  void OnSetCookie(base::OnceCallback<void(bool)> callback,
                   std::unique_ptr<network::SetCookieResult> result);
  void OnHasCookie(base::OnceCallback<void(bool)> callback,
                   std::unique_ptr<network::GetCookiesResult> result);

  // Weak pointer is fine here since it must outlive this web controller, which
  // is guaranteed by the owner of this object.
  content::WebContents* web_contents_;
  std::unique_ptr<DevtoolsClient> devtools_client_;

  base::WeakPtrFactory<WebController> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebController);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CONTROLLER_H_

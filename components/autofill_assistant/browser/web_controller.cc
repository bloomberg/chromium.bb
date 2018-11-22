// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web_controller.h"

#include <math.h>
#include <algorithm>
#include <ctime>
#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
using autofill::ContentAutofillDriver;

namespace {
// Time between two periodic box model checks.
static constexpr base::TimeDelta kPeriodicBoxModelCheckInterval =
    base::TimeDelta::FromMilliseconds(200);

// Timeout after roughly 10 seconds (50*200ms).
static int kPeriodicBoxModelCheckRounds = 50;

// Expiration time for the Autofill Assistant cookie.
static int kCookieExpiresSeconds = 600;

// Name and value used for the static cookie.
const char* const kAutofillAssistantCookieName = "autofill_assistant_cookie";
const char* const kAutofillAssistantCookieValue = "true";

const char* const kGetBoundingClientRectAsList =
    R"(function(node) {
      const r = node.getBoundingClientRect();
      const v = window.visualViewport;
      return [r.left, r.top, r.right, r.bottom,
              v.offsetLeft, v.offsetTop, v.width, v.height];
    })";

const char* const kScrollIntoViewScript =
    R"(function(node) {
    const rect = node.getBoundingClientRect();
    if (rect.height < window.innerHeight) {
      window.scrollBy({top: rect.top - window.innerHeight * 0.25,
        behavior: 'smooth'});
    } else {
      window.scrollBy({top: rect.top, behavior: 'smooth'});
    }
    node.scrollIntoViewIfNeeded();
  })";

const char* const kScrollIntoViewIfNeededScript =
    R"(function(node) {
    node.scrollIntoViewIfNeeded();
  })";

const char* const kScrollByScript =
    R"(window.scrollBy(%f * window.visualViewport.width,
                       %f * window.visualViewport.height))";

// Javascript to select a value from a select box. Also fires a "change" event
// to trigger any listeners. Changing the index directly does not trigger this.
const char* const kSelectOptionScript =
    R"(function(value) {
      const uppercaseValue = value.toUpperCase();
      var found = false;
      for (var i = 0; i < this.options.length; ++i) {
        const label = this.options[i].label.toUpperCase();
        if (label.length > 0 && label.startsWith(uppercaseValue)) {
          this.options.selectedIndex = i;
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
      const e = document.createEvent('HTMLEvents');
      e.initEvent('change', true, true);
      this.dispatchEvent(e);
      return true;
    })";

// Javascript to highlight an element.
const char* const kHighlightElementScript =
    R"(function() {
      this.style.boxShadow = '0px 0px 0px 3px white, ' +
          '0px 0px 0px 6px rgb(66, 133, 244)';
      return true;
    })";

// Javascript code to retrieve the 'value' attribute of a node.
const char* const kGetValueAttributeScript =
    "function () { return this.value; }";

// Javascript code to set the 'value' attribute of a node and then fire a
// "change" event to trigger any listeners.
const char* const kSetValueAttributeScript =
    R"(function (value) {
         this.value = value;
         const e = document.createEvent('HTMLEvents');
         e.initEvent('change', true, true);
         this.dispatchEvent(e);
       })";

// Javascript code to set an attribute of a node to a given value.
const char* const kSetAttributeScript =
    R"(function (attribute, value) {
         let receiver = this;
         for (let i = 0; i < attribute.length - 1; i++) {
           receiver = receiver[attribute[i]];
         }
         receiver[attribute[attribute.length - 1]] = value;
       })";

// Javascript code to get the outerHTML of a node.
// TODO(crbug.com/806868): Investigate if using DOM.GetOuterHtml would be a
// better solution than injecting Javascript code.
const char* const kGetOuterHtmlScript =
    "function () { return this.outerHTML; }";

// Javascript code to get document root element.
const char* const kGetDocumentElement =
    R"(
    (function() {
      return document.documentElement;
    }())
    )";

// Javascript code to query all elements for a selector.
const char* const kQuerySelectorAll =
    R"(function (selector, strictMode) {
      var found = this.querySelectorAll(selector);
      if(found.length == 1)
        return found[0];
      if(found.length > 1 && !strictMode)
        return found[0];
      return undefined;
    })";
}  // namespace

WebController::ElementPositionGetter::ElementPositionGetter()
    : visual_state_updated_(false), weak_ptr_factory_(this) {}
WebController::ElementPositionGetter::~ElementPositionGetter() = default;

void WebController::ElementPositionGetter::Start(
    content::RenderFrameHost* frame_host,
    DevtoolsClient* devtools_client,
    std::string element_object_id,
    base::OnceCallback<void(int, int)> callback) {
  callback_ = std::move(callback);

  // Wait for a roundtrips through the renderer and compositor pipeline,
  // otherwise touch event may be dropped because of missing handler.
  // Note that mouse left button will always be send to the renderer, but it
  // is slightly better to wait for the changes, like scroll, to be visualized
  // in compositor as real interaction.
  frame_host->InsertVisualStateCallback(base::BindOnce(
      &WebController::ElementPositionGetter::OnVisualStateUpdatedCallback,
      weak_ptr_factory_.GetWeakPtr()));

  // Set 'point_x' and 'point_y' to -1 to force one round of stable check.
  GetAndWaitBoxModelStable(devtools_client, element_object_id,
                           /* point_x= */ -1,
                           /* point_y= */ -1, kPeriodicBoxModelCheckRounds);
}

void WebController::ElementPositionGetter::OnVisualStateUpdatedCallback(
    bool state) {
  if (state) {
    visual_state_updated_ = true;
    return;
  }

  OnResult(-1, -1);
}

void WebController::ElementPositionGetter::GetAndWaitBoxModelStable(
    DevtoolsClient* devtools_client,
    std::string object_id,
    int point_x,
    int point_y,
    int remaining_rounds) {
  devtools_client->GetDOM()->GetBoxModel(
      dom::GetBoxModelParams::Builder().SetObjectId(object_id).Build(),
      base::BindOnce(
          &WebController::ElementPositionGetter::OnGetBoxModelForStableCheck,
          weak_ptr_factory_.GetWeakPtr(), devtools_client, object_id, point_x,
          point_y, remaining_rounds));
}

void WebController::ElementPositionGetter::OnGetBoxModelForStableCheck(
    DevtoolsClient* devtools_client,
    std::string object_id,
    int point_x,
    int point_y,
    int remaining_rounds,
    std::unique_ptr<dom::GetBoxModelResult> result) {
  if (!result || !result->GetModel() || !result->GetModel()->GetContent()) {
    DLOG(ERROR) << "Failed to get box model.";
    OnResult(-1, -1);
    return;
  }

  // Return the center of the element.
  const std::vector<double>* content_box = result->GetModel()->GetContent();
  DCHECK_EQ(content_box->size(), 8u);
  int new_point_x =
      round((round((*content_box)[0]) + round((*content_box)[2])) * 0.5);
  int new_point_y =
      round((round((*content_box)[3]) + round((*content_box)[5])) * 0.5);

  // Wait for at least three rounds (~600ms =
  // 3*kPeriodicBoxModelCheckInterval) for visual state update callback since
  // it might take longer time to return or never return if no updates.
  DCHECK(kPeriodicBoxModelCheckRounds > 2 &&
         kPeriodicBoxModelCheckRounds >= remaining_rounds);
  if (new_point_x == point_x && new_point_y == point_y &&
      (visual_state_updated_ ||
       remaining_rounds + 2 < kPeriodicBoxModelCheckRounds)) {
    // Note that there is still a chance that the element's position has been
    // changed after the last call of GetBoxModel, however, it might be safe
    // to assume the element's position will not be changed before issuing
    // click or tap event after stable for kPeriodicBoxModelCheckInterval. In
    // addition, checking again after issuing click or tap event doesn't help
    // since the change may be expected.
    OnResult(new_point_x, new_point_y);
    return;
  }

  if (remaining_rounds <= 0) {
    OnResult(-1, -1);
    return;
  }

  // Scroll the element into view again if it was moved out of view.
  // Check 'point_x' amd 'point_y' are greater or equal than zero to escape the
  // first round.
  if (point_x >= 0 && point_y >= 0) {
    std::vector<std::unique_ptr<runtime::CallArgument>> argument;
    argument.emplace_back(
        runtime::CallArgument::Builder().SetObjectId(object_id).Build());
    devtools_client->GetRuntime()->CallFunctionOn(
        runtime::CallFunctionOnParams::Builder()
            .SetObjectId(object_id)
            .SetArguments(std::move(argument))
            .SetFunctionDeclaration(std::string(kScrollIntoViewIfNeededScript))
            .SetReturnByValue(true)
            .Build(),
        base::BindOnce(&WebController::ElementPositionGetter::OnScrollIntoView,
                       weak_ptr_factory_.GetWeakPtr(), devtools_client,
                       object_id, new_point_x, new_point_y, remaining_rounds));
    return;
  }

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &WebController::ElementPositionGetter::GetAndWaitBoxModelStable,
          weak_ptr_factory_.GetWeakPtr(), devtools_client, object_id,
          new_point_x, new_point_y, --remaining_rounds),
      kPeriodicBoxModelCheckInterval);
}

void WebController::ElementPositionGetter::OnScrollIntoView(
    DevtoolsClient* devtools_client,
    std::string object_id,
    int point_x,
    int point_y,
    int remaining_rounds,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to scroll the element.";
    OnResult(-1, -1);
    return;
  }

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &WebController::ElementPositionGetter::GetAndWaitBoxModelStable,
          weak_ptr_factory_.GetWeakPtr(), devtools_client, object_id, point_x,
          point_y, --remaining_rounds),
      kPeriodicBoxModelCheckInterval);
}

void WebController::ElementPositionGetter::OnResult(int x, int y) {
  if (callback_) {
    std::move(callback_).Run(x, y);
  }
}

// static
std::unique_ptr<WebController> WebController::CreateForWebContents(
    content::WebContents* web_contents) {
  return std::make_unique<WebController>(
      web_contents,
      std::make_unique<DevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents)));
}

WebController::WebController(content::WebContents* web_contents,
                             std::unique_ptr<DevtoolsClient> devtools_client)
    : web_contents_(web_contents),
      devtools_client_(std::move(devtools_client)),
      weak_ptr_factory_(this) {}

WebController::~WebController() {}

WebController::FillFormInputData::FillFormInputData() {}

WebController::FillFormInputData::~FillFormInputData() {}

const GURL& WebController::GetUrl() {
  return web_contents_->GetLastCommittedURL();
}

void WebController::LoadURL(const GURL& url) {
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
}

void WebController::ClickOrTapElement(const std::vector<std::string>& selectors,
                                      base::OnceCallback<void(bool)> callback) {
#if defined(OS_ANDROID)
  TapElement(selectors, std::move(callback));
#else
  // TODO(crbug.com/806868): Remove 'ClickElement' since this feature is only
  // available on Android.
  ClickElement(selectors, std::move(callback));
#endif
}

void WebController::ClickElement(const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(selectors, /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForClickOrTap,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), /* is_a_click= */ true));
}

void WebController::TapElement(const std::vector<std::string>& selectors,
                               base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(selectors, /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForClickOrTap,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), /* is_a_click= */ false));
}

void WebController::OnFindElementForClickOrTap(
    base::OnceCallback<void(bool)> callback,
    bool is_a_click,
    std::unique_ptr<FindElementResult> result) {
  // Found element must belong to a frame.
  if (!result->container_frame_host || result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to click or tap.";
    OnResult(false, std::move(callback));
    return;
  }

  ClickOrTapElement(std::move(result), is_a_click, std::move(callback));
}

void WebController::ClickOrTapElement(
    std::unique_ptr<FindElementResult> target_element,
    bool is_a_click,
    base::OnceCallback<void(bool)> callback) {
  std::string element_object_id = target_element->object_id;
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(element_object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnScrollIntoView,
                     weak_ptr_factory_.GetWeakPtr(), std::move(target_element),
                     std::move(callback), is_a_click));
}

void WebController::OnScrollIntoView(
    std::unique_ptr<FindElementResult> target_element,
    base::OnceCallback<void(bool)> callback,
    bool is_a_click,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to scroll the element.";
    OnResult(false, std::move(callback));
    return;
  }

  ElementPositionGetter* element_position_checker = new ElementPositionGetter();
  element_position_checker->Start(
      target_element->container_frame_host, devtools_client_.get(),
      target_element->object_id,
      base::BindOnce(&WebController::TapOrClickOnCoordinates,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::WrapUnique(element_position_checker),
                     std::move(callback), is_a_click));
}

void WebController::TapOrClickOnCoordinates(
    std::unique_ptr<ElementPositionGetter> element_position_getter,
    base::OnceCallback<void(bool)> callback,
    bool is_a_click,
    int x,
    int y) {
  if (x < 0 || y < 0) {
    DLOG(ERROR) << "Failed to get element position.";
    OnResult(false, std::move(callback));
    return;
  }

  if (is_a_click) {
    devtools_client_->GetInput()->DispatchMouseEvent(
        input::DispatchMouseEventParams::Builder()
            .SetX(x)
            .SetY(y)
            .SetClickCount(1)
            .SetButton(input::DispatchMouseEventButton::LEFT)
            .SetType(input::DispatchMouseEventType::MOUSE_PRESSED)
            .Build(),
        base::BindOnce(&WebController::OnDispatchPressMouseEvent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback), x,
                       y));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  touch_points.emplace_back(
      input::TouchPoint::Builder().SetX(x).SetY(y).Build());
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_START)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      base::BindOnce(&WebController::OnDispatchTouchEventStart,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchPressMouseEvent(
    base::OnceCallback<void(bool)> callback,
    int x,
    int y,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  if (!result) {
    DLOG(ERROR) << "Failed to dispatch mouse left button pressed event.";
    OnResult(false, std::move(callback));
    return;
  }

  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::DispatchMouseEventButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_RELEASED)
          .Build(),
      base::BindOnce(&WebController::OnDispatchReleaseMouseEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchReleaseMouseEvent(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  OnResult(true, std::move(callback));
}

void WebController::OnDispatchTouchEventStart(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  if (!result) {
    DLOG(ERROR) << "Failed to dispatch touch start event.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_END)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      base::BindOnce(&WebController::OnDispatchTouchEventEnd,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchTouchEventEnd(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  DCHECK(result);
  OnResult(true, std::move(callback));
}

void WebController::ElementCheck(ElementCheckType check_type,
                                 const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  // We don't use strict_mode because we only check for the existence of at
  // least one such element and we don't act on it.
  FindElement(selectors, /* strict_mode= */ false,
              base::BindOnce(&WebController::OnFindElementForCheck,
                             weak_ptr_factory_.GetWeakPtr(), check_type,
                             std::move(callback)));
}

void WebController::OnFindElementForCheck(
    ElementCheckType check_type,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> result) {
  if (result->object_id.empty()) {
    OnResult(false, std::move(callback));
    return;
  }
  if (check_type == kExistenceCheck) {
    OnResult(true, std::move(callback));
    return;
  }
  DCHECK_EQ(check_type, kVisibilityCheck);

  devtools_client_->GetDOM()->GetBoxModel(
      dom::GetBoxModelParams::Builder().SetObjectId(result->object_id).Build(),
      base::BindOnce(&WebController::OnGetBoxModelForVisible,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetBoxModelForVisible(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<dom::GetBoxModelResult> result) {
  OnResult(result && result->GetModel() && result->GetModel()->GetContent(),
           std::move(callback));
}

void WebController::FindElement(const std::vector<std::string>& selectors,
                                bool strict_mode,
                                FindElementCallback callback) {
  devtools_client_->GetRuntime()->Evaluate(
      std::string(kGetDocumentElement),
      base::BindOnce(&WebController::OnGetDocumentElement,
                     weak_ptr_factory_.GetWeakPtr(), selectors, strict_mode,
                     std::move(callback)));
}

void WebController::OnGetDocumentElement(
    const std::vector<std::string>& selectors,
    bool strict_mode,
    FindElementCallback callback,
    std::unique_ptr<runtime::EvaluateResult> result) {
  std::unique_ptr<FindElementResult> element_result =
      std::make_unique<FindElementResult>();
  element_result->container_frame_host = web_contents_->GetMainFrame();
  element_result->container_frame_selector_index = 0;
  element_result->object_id = "";
  if (!result || !result->GetResult() || !result->GetResult()->HasObjectId()) {
    DLOG(ERROR) << "Failed to get document root element.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  RecursiveFindElement(result->GetResult()->GetObjectId(), 0, selectors,
                       strict_mode, std::move(element_result),
                       std::move(callback));
}

void WebController::RecursiveFindElement(
    const std::string& object_id,
    size_t index,
    const std::vector<std::string>& selectors,
    bool strict_mode,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(runtime::CallArgument::Builder()
                            .SetValue(base::Value::ToUniquePtrValue(
                                base::Value(selectors[index])))
                            .Build());
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(strict_mode)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kQuerySelectorAll))
          .Build(),
      base::BindOnce(&WebController::OnQuerySelectorAll,
                     weak_ptr_factory_.GetWeakPtr(), index, selectors,
                     strict_mode, std::move(element_result),
                     std::move(callback)));
}

void WebController::OnQuerySelectorAll(
    size_t index,
    const std::vector<std::string>& selectors,
    bool strict_mode,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || !result->GetResult() || !result->GetResult()->HasObjectId()) {
    std::move(callback).Run(std::move(element_result));
    return;
  }

  // Return object id of the element.
  if (selectors.size() == index + 1) {
    element_result->object_id = result->GetResult()->GetObjectId();
    std::move(callback).Run(std::move(element_result));
    return;
  }

  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder()
          .SetObjectId(result->GetResult()->GetObjectId())
          .Build(),
      base::BindOnce(
          &WebController::OnDescribeNode, weak_ptr_factory_.GetWeakPtr(),
          result->GetResult()->GetObjectId(), index, selectors, strict_mode,
          std::move(element_result), std::move(callback)));
}

void WebController::OnDescribeNode(
    const std::string& object_id,
    size_t index,
    const std::vector<std::string>& selectors,
    bool strict_mode,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DLOG(ERROR) << "Failed to describe the node.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;
  if (node->HasContentDocument()) {
    backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());

    element_result->container_frame_selector_index = index;

    // Find out the corresponding render frame host through document url and
    // name.
    // TODO(crbug.com/806868): Use more attributes to find out the render frame
    // host if name and document url are not enough to uniquely identify it.
    std::string frame_name;
    if (node->HasAttributes()) {
      const std::vector<std::string>* attributes = node->GetAttributes();
      for (size_t i = 0; i < attributes->size();) {
        if ((*attributes)[i] == "name") {
          frame_name = (*attributes)[i + 1];
          break;
        }
        // Jump two positions since attribute name and value are always paired.
        i = i + 2;
      }
    }
    element_result->container_frame_host = FindCorrespondingRenderFrameHost(
        frame_name, node->GetContentDocument()->GetDocumentURL());
    if (!element_result->container_frame_host) {
      DLOG(ERROR) << "Failed to find corresponding owner frame.";
      std::move(callback).Run(std::move(element_result));
      return;
    }
  } else if (node->HasFrameId()) {
    // TODO(crbug.com/806868): Support out-of-process iframe.
    DLOG(WARNING) << "The element is inside an OOPIF.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  if (node->HasShadowRoots()) {
    // TODO(crbug.com/806868): Support multiple shadow roots.
    backend_ids.emplace_back(
        node->GetShadowRoots()->front()->GetBackendNodeId());
  }

  if (!backend_ids.empty()) {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetBackendNodeId(backend_ids[0])
            .Build(),
        base::BindOnce(&WebController::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr(), index, selectors,
                       strict_mode, std::move(element_result),
                       std::move(callback)));
    return;
  }

  RecursiveFindElement(object_id, ++index, selectors, strict_mode,
                       std::move(element_result), std::move(callback));
}

void WebController::OnResolveNode(
    size_t index,
    const std::vector<std::string>& selectors,
    bool strict_mode,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() || !result->GetObject()->HasObjectId()) {
    DLOG(ERROR) << "Failed to resolve object id from backend id.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  RecursiveFindElement(result->GetObject()->GetObjectId(), ++index, selectors,
                       strict_mode, std::move(element_result),
                       std::move(callback));
}

content::RenderFrameHost* WebController::FindCorrespondingRenderFrameHost(
    std::string name,
    std::string document_url) {
  content::RenderFrameHost* ret_frame = nullptr;
  for (auto* frame : web_contents_->GetAllFrames()) {
    if (frame->GetFrameName() == name &&
        frame->GetLastCommittedURL().spec() == document_url) {
      DCHECK(!ret_frame);
      ret_frame = frame;
    }
  }

  return ret_frame;
}

void WebController::OnResult(bool result,
                             base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(result);
}

void WebController::OnResult(
    bool exists,
    const std::string& value,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  std::move(callback).Run(exists, value);
}

void WebController::OnFindElementForFocusElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  if (element_result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to focus on.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(runtime::CallArgument::Builder()
                            .SetObjectId(element_result->object_id)
                            .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFocusElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to focus on element.";
    OnResult(false, std::move(callback));
    return;
  }
  OnResult(true, std::move(callback));
}

void WebController::FillAddressForm(const autofill::AutofillProfile* profile,
                                    const std::vector<std::string>& selectors,
                                    base::OnceCallback<void(bool)> callback) {
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->profile =
      std::make_unique<autofill::AutofillProfile>(*profile);
  FindElement(selectors,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(data_to_autofill), selectors,
                             std::move(callback)));
}

void WebController::OnFindElementForFillingForm(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  if (element_result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element for filling the form.";
    OnResult(false, std::move(callback));
    return;
  }

  ContentAutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
      element_result->container_frame_host);
  DCHECK(!selectors.empty());
  // TODO(crbug.com/806868): Figure out whether there are cases where we need
  // more than one selector, and come up with a solution that can figure out the
  // right number of selectors to include.
  driver->GetAutofillAgent()->GetElementFormAndFieldData(
      std::vector<std::string>(1, selectors.back()),
      base::BindOnce(&WebController::OnGetFormAndFieldDataForFillingForm,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(data_to_autofill), std::move(callback),
                     element_result->container_frame_host));
}

void WebController::OnGetFormAndFieldDataForFillingForm(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    base::OnceCallback<void(bool)> callback,
    content::RenderFrameHost* container_frame_host,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  if (form_data.fields.empty()) {
    DLOG(ERROR) << "Failed to get form data to fill form.";
    OnResult(false, std::move(callback));
    return;
  }

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(container_frame_host);
  if (!driver) {
    DLOG(ERROR) << "Failed to get the autofill driver.";
    OnResult(false, std::move(callback));
    return;
  }

  if (data_to_autofill->card) {
    driver->autofill_manager()->FillCreditCardForm(
        autofill::kNoQueryId, form_data, form_field, *data_to_autofill->card,
        data_to_autofill->cvc);
  } else {
    driver->autofill_manager()->FillProfileForm(*data_to_autofill->profile,
                                                form_data, form_field);
  }

  OnResult(true, std::move(callback));
}

void WebController::FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                                 const base::string16& cvc,
                                 const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->card = std::move(card);
  data_to_autofill->cvc = cvc;
  FindElement(selectors,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(data_to_autofill), selectors,
                             std::move(callback)));
}

void WebController::SelectOption(const std::vector<std::string>& selectors,
                                 const std::string& selected_option,
                                 base::OnceCallback<void(bool)> callback) {
  FindElement(selectors,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSelectOption,
                             weak_ptr_factory_.GetWeakPtr(), selected_option,
                             std::move(callback)));
}

void WebController::OnFindElementForSelectOption(
    const std::string& selected_option,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to select an option.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(selected_option)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSelectOptionScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnSelectOption,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSelectOption(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to select option.";
    OnResult(false, std::move(callback));
    return;
  }

  // Read the result returned from Javascript code.
  DCHECK(result->GetResult()->GetValue()->is_bool());
  OnResult(result->GetResult()->GetValue()->GetBool(), std::move(callback));
}

void WebController::HighlightElement(const std::vector<std::string>& selectors,
                                     base::OnceCallback<void(bool)> callback) {
  FindElement(
      selectors,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForHighlightElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to highlight.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kHighlightElementScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnHighlightElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to highlight element.";
    OnResult(false, std::move(callback));
    return;
  }
  // Read the result returned from Javascript code.
  DCHECK(result->GetResult()->GetValue()->is_bool());
  OnResult(result->GetResult()->GetValue()->GetBool(), std::move(callback));
}

void WebController::FocusElement(const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(
      selectors,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::GetFieldValue(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  FindElement(
      selectors,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForGetFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForGetFieldValue(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    OnResult(/* exists= */ false, "", std::move(callback));
    return;
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetFunctionDeclaration(std::string(kGetValueAttributeScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetValueAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetValueAttribute(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || result->HasExceptionDetails()) {
    OnResult(/* exists= */ true, "", std::move(callback));
    return;
  }

  // Read the result returned from Javascript code.
  DCHECK(result->GetResult()->GetValue()->is_string());
  OnResult(/* exists= */ true, result->GetResult()->GetValue()->GetString(),
           std::move(callback));
}

void WebController::SetFieldValue(const std::vector<std::string>& selectors,
                                  const std::string& value,
                                  bool simulate_key_presses,
                                  base::OnceCallback<void(bool)> callback) {
  if (simulate_key_presses) {
    // We first clear the field value, and then simulate the key presses.
    // TODO(crbug.com/806868): Disable keyboard during this action and then
    // reset to previous state.
    InternalSetFieldValue(
        selectors, "",
        base::BindOnce(&WebController::OnClearFieldForDispatchKeyEvent,
                       weak_ptr_factory_.GetWeakPtr(), selectors, value,
                       std::move(callback)));
    return;
  }

  InternalSetFieldValue(selectors, value, std::move(callback));
}

void WebController::InternalSetFieldValue(
    const std::vector<std::string>& selectors,
    const std::string& value,
    base::OnceCallback<void(bool)> callback) {
  FindElement(selectors,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSetFieldValue,
                             weak_ptr_factory_.GetWeakPtr(), value,
                             std::move(callback)));
}

void WebController::OnClearFieldForDispatchKeyEvent(
    const std::vector<std::string>& selectors,
    const std::string& value,
    base::OnceCallback<void(bool)> callback,
    bool clear_status) {
  if (!clear_status) {
    OnResult(false, std::move(callback));
    return;
  }

  // TODO(crbug.com/806868): Substitute mouse click with touch tap.
  //
  // Note that 'KeyDown' will not be handled by the element immediately after
  // touch tap. Add ~1 second delay before 'DispatchKeyDownEvent' in
  // 'OnClickOrTapElementForDispatchKeyEvent' solved the problem, needs more
  // investigation for this timing issue. One possible reason is that events
  // from different devices are not guarranteed to be handled in order (needs a
  // way to make sure previous events have been handled).
  ClickElement(selectors,
               base::BindOnce(&WebController::OnClickElementForDispatchKeyEvent,
                              weak_ptr_factory_.GetWeakPtr(), value,
                              std::move(callback)));
}

void WebController::OnClickElementForDispatchKeyEvent(
    const std::string& value,
    base::OnceCallback<void(bool)> callback,
    bool click_status) {
  if (!click_status) {
    OnResult(false, std::move(callback));
    return;
  }

  DispatchKeyDownEvent(value, 0, std::move(callback));
}

void WebController::DispatchKeyDownEvent(
    const std::string& value,
    size_t index,
    base::OnceCallback<void(bool)> callback) {
  if (index >= value.size()) {
    OnResult(true, std::move(callback));
    return;
  }

  devtools_client_->GetInput()->DispatchKeyEvent(
      input::DispatchKeyEventParams::Builder()
          .SetType(input::DispatchKeyEventType::KEY_DOWN)
          .SetText(std::string(1, value[index]))
          .Build(),
      base::BindOnce(&WebController::DispatchKeyUpEvent,
                     weak_ptr_factory_.GetWeakPtr(), value, index,
                     std::move(callback)));
}

void WebController::DispatchKeyUpEvent(
    const std::string& value,
    size_t index,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_LT(index, value.size());
  devtools_client_->GetInput()->DispatchKeyEvent(
      input::DispatchKeyEventParams::Builder()
          .SetType(input::DispatchKeyEventType::KEY_UP)
          .SetText(std::string(1, value[index]))
          .Build(),
      base::BindOnce(&WebController::OnDispatchKeyUpEvent,
                     weak_ptr_factory_.GetWeakPtr(), value, index,
                     std::move(callback)));
}

void WebController::OnDispatchKeyUpEvent(
    const std::string& value,
    size_t index,
    base::OnceCallback<void(bool)> callback) {
  DispatchKeyDownEvent(value, index + 1, std::move(callback));
}

void WebController::OnFindElementForSetFieldValue(
    const std::string& value,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(value)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSetValueAttributeScript))
          .Build(),
      base::BindOnce(&WebController::OnSetValueAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetValueAttribute(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  OnResult(result && !result->HasExceptionDetails(), std::move(callback));
}

void WebController::SetAttribute(const std::vector<std::string>& selectors,
                                 const std::vector<std::string>& attribute,
                                 const std::string& value,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK_GT(selectors.size(), 0u);
  DCHECK_GT(attribute.size(), 0u);
  FindElement(selectors,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSetAttribute,
                             weak_ptr_factory_.GetWeakPtr(), attribute, value,
                             std::move(callback)));
}

void WebController::OnFindElementForSetAttribute(
    const std::vector<std::string>& attribute,
    const std::string& value,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    OnResult(false, std::move(callback));
    return;
  }

  base::Value::ListStorage attribute_values;
  for (const std::string& string : attribute) {
    attribute_values.emplace_back(base::Value(string));
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(base::Value::ToUniquePtrValue(
                                 base::Value(attribute_values)))
                             .Build());
  arguments.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(value)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kSetAttributeScript))
          .Build(),
      base::BindOnce(&WebController::OnSetAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetAttribute(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  OnResult(result && !result->HasExceptionDetails(), std::move(callback));
}

void WebController::GetOuterHtml(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  FindElement(
      selectors,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::unique_ptr<BatchElementChecker>
WebController::CreateBatchElementChecker() {
  return std::make_unique<BatchElementChecker>(this);
}

void WebController::GetElementPosition(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool, const RectF&)> callback) {
  FindElement(
      selectors, /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForPosition,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::ScrollBy(float distanceXRatio, float distanceYRatio) {
  devtools_client_->GetRuntime()->Evaluate(
      base::StringPrintf(kScrollByScript, distanceXRatio, distanceYRatio),
      base::DoNothing());
}

void WebController::OnFindElementForPosition(
    base::OnceCallback<void(bool, const RectF&)> callback,
    std::unique_ptr<FindElementResult> result) {
  if (result->object_id.empty()) {
    RectF empty;
    std::move(callback).Run(false, empty);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(result->object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kGetBoundingClientRectAsList))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetElementPositionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetElementPositionResult(
    base::OnceCallback<void(bool, const RectF&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || result->HasExceptionDetails()) {
    RectF empty;
    std::move(callback).Run(false, empty);
    return;
  }
  const auto* value = result->GetResult()->GetValue();
  DCHECK(value);
  DCHECK(value->is_list());
  const auto& list = value->GetList();
  DCHECK_EQ(list.size(), 8u);

  // getBoundingClientRect returns coordinates in the layout viewport. They need
  // to be transformed into coordinates in the visual viewport, between 0 and 1.
  float left_layout = static_cast<float>(list[0].GetDouble());
  float top_layout = static_cast<float>(list[1].GetDouble());
  float right_layout = static_cast<float>(list[2].GetDouble());
  float bottom_layout = static_cast<float>(list[3].GetDouble());
  float visual_left_offset = static_cast<float>(list[4].GetDouble());
  float visual_top_offset = static_cast<float>(list[5].GetDouble());
  float visual_w = static_cast<float>(list[6].GetDouble());
  float visual_h = static_cast<float>(list[7].GetDouble());

  RectF rect;
  rect.left = std::max(0.0f, left_layout - visual_left_offset) / visual_w;
  rect.top = std::max(0.0f, top_layout - visual_top_offset) / visual_h;
  rect.right = std::max(0.0f, right_layout - visual_left_offset) / visual_w;
  rect.bottom = std::max(0.0f, bottom_layout - visual_top_offset) / visual_h;

  std::move(callback).Run(true, rect);
}

void WebController::OnFindElementForGetOuterHtml(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    OnResult(false, "", std::move(callback));
    return;
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetFunctionDeclaration(std::string(kGetOuterHtmlScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetOuterHtml(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result || result->HasExceptionDetails()) {
    OnResult(false, "", std::move(callback));
    return;
  }

  // Read the result returned from Javascript code.
  DCHECK(result->GetResult()->GetValue()->is_string());
  OnResult(true, result->GetResult()->GetValue()->GetString(),
           std::move(callback));
}

void WebController::SetCookie(const std::string& domain,
                              base::OnceCallback<void(bool)> callback) {
  DCHECK(!domain.empty());
  auto expires_seconds =
      std::chrono::seconds(std::time(nullptr)).count() + kCookieExpiresSeconds;
  devtools_client_->GetNetwork()->SetCookie(
      network::SetCookieParams::Builder()
          .SetName(kAutofillAssistantCookieName)
          .SetValue(kAutofillAssistantCookieValue)
          .SetDomain(domain)
          .SetExpires(expires_seconds)
          .Build(),
      base::BindOnce(&WebController::OnSetCookie,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetCookie(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<network::SetCookieResult> result) {
  std::move(callback).Run(result && result->GetSuccess());
}

void WebController::HasCookie(base::OnceCallback<void(bool)> callback) {
  devtools_client_->GetNetwork()->GetCookies(
      base::BindOnce(&WebController::OnHasCookie,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnHasCookie(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<network::GetCookiesResult> result) {
  if (!result) {
    std::move(callback).Run(false);
    return;
  }

  const auto& cookies = *result->GetCookies();
  for (const auto& cookie : cookies) {
    if (cookie->GetName() == kAutofillAssistantCookieName &&
        cookie->GetValue() == kAutofillAssistantCookieValue) {
      std::move(callback).Run(true);
      return;
    }
  }
  std::move(callback).Run(false);
}

void WebController::ClearCookie() {
  devtools_client_->GetNetwork()->DeleteCookies(kAutofillAssistantCookieName,
                                                base::DoNothing());
}

}  // namespace autofill_assistant

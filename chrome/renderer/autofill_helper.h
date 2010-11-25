// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_HELPER_H_
#define CHROME_RENDERER_AUTOFILL_HELPER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "chrome/renderer/form_manager.h"
#include "chrome/renderer/page_click_listener.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"

class RenderView;

namespace WebKit {
class WebInputElement;
class WebKeyboardEvent;
class WebString;
}

// AutoFillHelper deals with AutoFill related communications between WebKit and
// the browser.  There is one AutofillHelper per RenderView.
// This code was originally part of RenderView.
// Note that AutoFill encompasses:
// - single text field suggestions, that we usually refer to as Autocomplete
// - entire form fill based on one field entry, referred to as form AutoFill.

class AutoFillHelper : public PageClickListener {
 public:
  explicit AutoFillHelper(RenderView* render_view);

  // Removes the Autocomplete suggestion |value| for the field named |name|.
  void RemoveAutocompleteSuggestion(const WebKit::WebString& name,
                                    const WebKit::WebString& value);

  // Called when we have received AutoFill suggestions from the browser.
  void SuggestionsReceived(int query_id,
                           const std::vector<string16>& values,
                           const std::vector<string16>& labels,
                           const std::vector<string16>& icons,
                           const std::vector<int>& unique_ids);

  // Called when we have received suggestions for an entire form from the
  // browser.
  void FormDataFilled(int query_id, const webkit_glue::FormData& form);

  // Called by Webkit when the user has selected a suggestion in the popup (this
  // happens when the user hovers over an suggestion or navigates the popup with
  // the arrow keys).
  void DidSelectAutoFillSuggestion(const WebKit::WebNode& node,
                                   int unique_id);

  // Called by Webkit when the user has accepted a suggestion in the popup.
  void DidAcceptAutoFillSuggestion(const WebKit::WebNode& node,
                                   const WebKit::WebString& value,
                                   int unique_id,
                                   unsigned index);

  // Called by WebKit when the user has cleared the selection from the AutoFill
  // suggestions popup.  This happens when a user uses the arrow keys to
  // navigate outside the range of possible selections, or when the popup
  // closes.
  void DidClearAutoFillSelection(const WebKit::WebNode& node);

  // Called when the frame contents are available.  Extracts the forms from that
  // frame and sends them to the browser for parsing.
  void FrameContentsAvailable(WebKit::WebFrame* frame);

  // Called before a frame is closed. Gives us an opportunity to clean up.
  // DEPRECATED.
  void FrameWillClose(WebKit::WebFrame* frame);

  // Called when |frame| is detached from the view. Gives us an opportunity to
  // clean up.
  void FrameDetached(WebKit::WebFrame* frame);

  // WebViewClient editor call forwarded by the RenderView.
  void TextDidChangeInTextField(const WebKit::WebInputElement& element);

  // WebViewClient editor call forwarded by the RenderView.  For lower level
  // event translation.  Specifically, for down/up key presses in an input
  // element.
  void KeyDownInTextField(const WebKit::WebInputElement& element,
                          const WebKit::WebKeyboardEvent& event);

 private:
  enum AutoFillAction {
    AUTOFILL_NONE,     // No state set.
    AUTOFILL_FILL,     // Fill the AutoFill form data.
    AUTOFILL_PREVIEW,  // Preview the AutoFill form data.
  };

  // PageClickListener implementation:
  virtual bool InputElementClicked(const WebKit::WebInputElement& element,
                                   bool was_focused,
                                   bool is_focused);

  // Shows the autocomplete suggestions for |element|.
  // This call is asynchronous and may or may not lead to the showing of a
  // suggestion popup (no popup is shown if there are no available suggestions).
  // |autofill_on_empty_values| specifies whether suggestions should be shown
  // when |element| contains no text.
  // |requires_caret_at_end| specifies whether suggestions should be shown when
  // the caret is not after the last character in |element|.
  // |display_warning_if_disabled| specifies whether a warning should be
  // displayed to the user if AutoFill has suggestions available, but cannot
  // fill them because it is disabled (e.g. when trying to fill a credit card
  // form on a non-secure website).
  void ShowSuggestions(const WebKit::WebInputElement& element,
                       bool autofill_on_empty_values,
                       bool requires_caret_at_end,
                       bool display_warning_if_disabled);

  // Queries the browser for Autocomplete and AutoFill suggestions for the given
  // |node|.
  void QueryAutoFillSuggestions(const WebKit::WebNode& node,
                                bool display_warning_if_disabled);

  // Queries the AutoFillManager for form data for the form containing |node|.
  // |value| is the current text in the field, and |unique_id| is the selected
  // profile's unique ID.  |action| specifies whether to Fill or Preview the
  // values returned from the AutoFillManager.
  void FillAutoFillFormData(const WebKit::WebNode& node,
                            int unique_id,
                            AutoFillAction action);

  // Scans the given frame for forms and sends them up to the browser.
  void SendForms(WebKit::WebFrame* frame);

  // Weak reference.
  RenderView* render_view_;

  FormManager form_manager_;

  // The ID of the last request sent for form field AutoFill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // The node corresponding to the last request sent for form field AutoFill.
  WebKit::WebNode autofill_query_node_;

  // The action to take when receiving AutoFill data from the AutoFillManager.
  AutoFillAction autofill_action_;

  // Should we display a warning if autofill is disabled?
  bool display_warning_if_disabled_;

  // Was the query node autofilled prior to previewing the form?
  bool was_query_node_autofilled_;

  // The menu index of the "Clear" menu item.
  int suggestions_clear_index_;

  // The menu index of the "AutoFill options..." menu item.
  int suggestions_options_index_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillHelper);
};

#endif  // CHROME_RENDERER_AUTOFILL_HELPER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card_result.h"

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"

namespace app_list {

AnswerCardResult::AnswerCardResult(Profile* profile,
                                   AppListControllerDelegate* list_controller,
                                   const std::string& result_url,
                                   const base::string16& result_title,
                                   views::View* web_view,
                                   content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      profile_(profile),
      list_controller_(list_controller),
      mouse_event_callback_(base::Bind(&AnswerCardResult::HandleMouseEvent,
                                       base::Unretained(this))) {
  set_display_type(DISPLAY_CARD);
  set_id(result_url);
  set_relevance(1);
  set_view(web_view);
  set_title(result_title);
  // web_contents may be null if the result is being duplicated after the
  // search provider's WebContents was destroyed.
  if (web_contents) {
    content::RenderViewHost* const rvh = web_contents->GetRenderViewHost();
    if (rvh) {
      rvh->GetWidget()->AddMouseEventCallback(mouse_event_callback_);
    }
  }
}

AnswerCardResult::~AnswerCardResult() {
  // WebContentsObserver::web_contents() returns nullptr after destruction of
  // WebContents.
  if (web_contents()) {
    content::RenderViewHost* const rvh = web_contents()->GetRenderViewHost();
    if (rvh) {
      rvh->GetWidget()->RemoveMouseEventCallback(mouse_event_callback_);
    }
  }
}

std::unique_ptr<SearchResult> AnswerCardResult::Duplicate() const {
  return base::MakeUnique<AnswerCardResult>(profile_, list_controller_, id(),
                                            title(), view(), web_contents());
}

void AnswerCardResult::Open(int event_flags) {
  list_controller_->OpenURL(profile_, GURL(id()), ui::PAGE_TRANSITION_GENERATED,
                            ui::DispositionFromEventFlags(event_flags));
}

bool AnswerCardResult::HandleMouseEvent(const blink::WebMouseEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::kMouseMove:
    case blink::WebInputEvent::kMouseEnter:
      if (!is_mouse_in_view())
        SetIsMouseInView(true);
      break;
    case blink::WebInputEvent::kMouseLeave:
      if (is_mouse_in_view())
        SetIsMouseInView(false);
      break;
    default:
      break;
  }

  return false;
}

}  // namespace app_list

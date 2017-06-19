// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_WEB_CONTENTS_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_WEB_CONTENTS_H_

#include <memory>

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_contents.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace views {
class WebView;
}

namespace app_list {

// Web source of contents for AnswerCardSearchProvider.
class AnswerCardWebContents : public AnswerCardContents,
                              public content::WebContentsDelegate,
                              public content::WebContentsObserver {
 public:
  explicit AnswerCardWebContents(Profile* profile);
  ~AnswerCardWebContents() override;

  // AnswerCardContents overrides:
  void LoadURL(const GURL& url) override;
  bool IsLoading() const override;
  views::View* GetView() override;

  // content::WebContentsDelegate overrides:
  void UpdatePreferredSize(content::WebContents* web_contents,
                           const gfx::Size& pref_size) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;
  void RenderViewCreated(content::RenderViewHost* host) override;
  void RenderViewDeleted(content::RenderViewHost* host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

 private:
  bool HandleMouseEvent(const blink::WebMouseEvent& event);
  void AttachToHost(content::RenderWidgetHost* host);
  void DetachFromHost();

  // Web view for the web contents managed by this class.
  const std::unique_ptr<views::WebView> web_view_;

  // Web contents managed by this class.
  const std::unique_ptr<content::WebContents> web_contents_;

  // Callbacks for mouse events in the web contents.
  const content::RenderWidgetHost::MouseEventCallback mouse_event_callback_;

  // Current widget host.
  content::RenderWidgetHost* host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardWebContents);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_WEB_CONTENTS_H_

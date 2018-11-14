// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

class MediaRouterDialogControllerViews;

class MediaRouterUiForTest
    : public content::WebContentsUserData<MediaRouterUiForTest>,
      public CastDialogView::Observer {
 public:
  static MediaRouterUiForTest* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  ~MediaRouterUiForTest() override;

  void ShowDialog();
  void HideDialog();
  bool IsDialogShown() const;

  // Chooses the source type in the dialog. Requires that the dialog is shown.
  void ChooseSourceType(CastDialogView::SourceType source_type);

  // These methods require that the dialog is shown and the specified sink is
  // shown in the dialog.
  void StartCasting(const MediaSink::Id& sink_id);
  void StopCasting(const MediaSink::Id& sink_id);
  // Stops casting to the first active sink found on the sink list. Requires
  // that such a sink exists.
  void StopCasting();

  // Waits until . Requires that the dialog is shown.
  void WaitForSink(const MediaSink::Id& sink_id);
  void WaitForAnyIssue();
  void WaitForAnyRoute();
  void WaitForDialogClosed();

  // These methods require that the dialog is shown, and the sink specified by
  // |sink_id| is in the dialog.
  std::string GetSinkName(const MediaSink::Id& sink_id) const;
  MediaRoute::Id GetRouteIdForSink(const MediaSink::Id& sink_id) const;
  std::string GetStatusTextForSink(const MediaSink::Id& sink_id) const;
  std::string GetIssueTextForSink(const MediaSink::Id& sink_id) const;

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  friend class content::WebContentsUserData<MediaRouterUiForTest>;

  enum class WatchType { kNone, kSink, kAnyIssue, kAnyRoute, kDialogClosed };

  explicit MediaRouterUiForTest(content::WebContents* web_contents);

  // CastDialogView::Observer:
  void OnDialogModelUpdated(CastDialogView* dialog_view) override;
  void OnDialogWillClose(CastDialogView* dialog_view) override;

  CastDialogSinkButton* GetSinkButton(const MediaSink::Id& sink_id) const;

  // Registers itself as an observer to the dialog, and waits until an event
  // of |watch_type| is observed. |sink_id| should be set only if observing for
  // a sink.
  void ObserveDialog(WatchType watch_type,
                     base::Optional<MediaSink::Id> sink_id = base::nullopt);

  content::WebContents* web_contents_;
  MediaRouterDialogControllerViews* dialog_controller_;

  base::Optional<MediaSink::Id> watch_sink_id_;
  base::Optional<base::OnceClosure> watch_callback_;
  WatchType watch_type_ = WatchType::kNone;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUiForTest);
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_

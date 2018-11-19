// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_ui_for_test.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"

namespace media_router {

namespace {

ui::MouseEvent CreateMousePressedEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                        gfx::Point(0, 0), ui::EventTimeForNow(),
                        ui::EF_LEFT_MOUSE_BUTTON, 0);
}

ui::MouseEvent CreateMouseReleasedEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(0, 0),
                        gfx::Point(0, 0), ui::EventTimeForNow(),
                        ui::EF_LEFT_MOUSE_BUTTON, 0);
}

}  // namespace

// static
MediaRouterUiForTest* MediaRouterUiForTest::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  // No-op if an instance already exists for the WebContents.
  MediaRouterUiForTest::CreateForWebContents(web_contents);
  return MediaRouterUiForTest::FromWebContents(web_contents);
}

MediaRouterUiForTest::~MediaRouterUiForTest() {
  CHECK(!watch_callback_);
}

void MediaRouterUiForTest::ShowDialog() {
  dialog_controller_->ShowMediaRouterDialog();
  base::RunLoop().RunUntilIdle();
}

void MediaRouterUiForTest::HideDialog() {
  dialog_controller_->HideMediaRouterDialog();
  base::RunLoop().RunUntilIdle();
}

bool MediaRouterUiForTest::IsDialogShown() const {
  return dialog_controller_->IsShowingMediaRouterDialog();
}

void MediaRouterUiForTest::ChooseSourceType(
    CastDialogView::SourceType source_type) {
  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);

  dialog_view->ButtonPressed(dialog_view->sources_button_for_test(),
                             CreateMousePressedEvent());
  int source_index;
  switch (source_type) {
    case CastDialogView::kTab:
      source_index = 0;
      break;
    case CastDialogView::kDesktop:
      source_index = 1;
      break;
    case CastDialogView::kLocalFile:
      source_index = 2;
      break;
  }
  dialog_view->sources_menu_model_for_test()->ActivatedAt(source_index);
}

void MediaRouterUiForTest::StartCasting(const MediaSink::Id& sink_id) {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_id);
  sink_button->OnMousePressed(CreateMousePressedEvent());
  sink_button->OnMouseReleased(CreateMouseReleasedEvent());
  base::RunLoop().RunUntilIdle();
}

void MediaRouterUiForTest::StopCasting(const MediaSink::Id& sink_id) {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_id);
  sink_button->icon_view()->OnMousePressed(CreateMousePressedEvent());
  sink_button->icon_view()->OnMouseReleased(CreateMouseReleasedEvent());
  base::RunLoop().RunUntilIdle();
}

void MediaRouterUiForTest::StopCasting() {
  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);
  for (CastDialogSinkButton* sink_button :
       dialog_view->sink_buttons_for_test()) {
    if (sink_button->sink().state == UIMediaSinkState::CONNECTED) {
      sink_button->icon_view()->OnMousePressed(CreateMousePressedEvent());
      sink_button->icon_view()->OnMouseReleased(CreateMouseReleasedEvent());
      base::RunLoop().RunUntilIdle();
      return;
    }
  }
  NOTREACHED() << "Sink was not found";
}

void MediaRouterUiForTest::WaitForSink(const MediaSink::Id& sink_id) {
  ObserveDialog(WatchType::kSink, sink_id);
}

void MediaRouterUiForTest::WaitForAnyIssue() {
  ObserveDialog(WatchType::kAnyIssue);
}

void MediaRouterUiForTest::WaitForAnyRoute() {
  ObserveDialog(WatchType::kAnyRoute);
}

void MediaRouterUiForTest::WaitForDialogClosed() {
  ObserveDialog(WatchType::kDialogClosed);
}

std::string MediaRouterUiForTest::GetSinkName(
    const MediaSink::Id& sink_id) const {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_id);
  return base::UTF16ToUTF8(sink_button->sink().friendly_name);
}

MediaRoute::Id MediaRouterUiForTest::GetRouteIdForSink(
    const MediaSink::Id& sink_id) const {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_id);
  if (!sink_button->sink().route) {
    NOTREACHED() << "Route not found for sink " << sink_id;
    return "";
  }
  return sink_button->sink().route->media_route_id();
}

std::string MediaRouterUiForTest::GetStatusTextForSink(
    const MediaSink::Id& sink_id) const {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_id);
  return base::UTF16ToUTF8(sink_button->sink().status_text);
}

std::string MediaRouterUiForTest::GetIssueTextForSink(
    const MediaSink::Id& sink_id) const {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_id);
  if (!sink_button->sink().issue) {
    NOTREACHED() << "Issue not found for sink " << sink_id;
    return "";
  }
  return sink_button->sink().issue->info().title;
}

MediaRouterUiForTest::MediaRouterUiForTest(content::WebContents* web_contents)
    : web_contents_(web_contents),
      dialog_controller_(
          MediaRouterDialogControllerViews::GetOrCreateForWebContents(
              web_contents)) {}

void MediaRouterUiForTest::OnDialogModelUpdated(CastDialogView* dialog_view) {
  if (!watch_callback_ || watch_type_ == WatchType::kDialogClosed)
    return;

  const std::vector<CastDialogSinkButton*>& sink_buttons =
      dialog_view->sink_buttons_for_test();
  if (std::find_if(sink_buttons.begin(), sink_buttons.end(),
                   [&, this](CastDialogSinkButton* sink_button) {
                     switch (watch_type_) {
                       case WatchType::kSink:
                         return sink_button->sink().id == *watch_sink_id_;
                       case WatchType::kAnyIssue:
                         return sink_button->sink().issue.has_value();
                       case WatchType::kAnyRoute:
                         return sink_button->sink().route.has_value();
                       case WatchType::kNone:
                       case WatchType::kDialogClosed:
                         NOTREACHED() << "Invalid WatchType";
                         return false;
                     }
                   }) != sink_buttons.end()) {
    std::move(*watch_callback_).Run();
    watch_callback_.reset();
    watch_sink_id_.reset();
    watch_type_ = WatchType::kNone;
    dialog_view->RemoveObserver(this);
  }
}

void MediaRouterUiForTest::OnDialogWillClose(CastDialogView* dialog_view) {
  if (watch_type_ == WatchType::kDialogClosed) {
    std::move(*watch_callback_).Run();
    watch_callback_.reset();
    watch_type_ = WatchType::kNone;
  }
  CHECK(!watch_callback_);
  if (dialog_view)
    dialog_view->RemoveObserver(this);
}

CastDialogSinkButton* MediaRouterUiForTest::GetSinkButton(
    const MediaSink::Id& sink_id) const {
  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);
  const std::vector<CastDialogSinkButton*>& sink_buttons =
      dialog_view->sink_buttons_for_test();
  auto it = std::find_if(sink_buttons.begin(), sink_buttons.end(),
                         [sink_id](CastDialogSinkButton* sink_button) {
                           return sink_button->sink().id == sink_id;
                         });
  if (it == sink_buttons.end()) {
    NOTREACHED() << "Sink button not found for sink ID: " << sink_id;
    return nullptr;
  } else {
    return *it;
  }
}

void MediaRouterUiForTest::ObserveDialog(
    WatchType watch_type,
    base::Optional<MediaSink::Id> sink_id) {
  CHECK(!watch_sink_id_);
  CHECK(!watch_callback_);
  CHECK_EQ(watch_type_, WatchType::kNone);
  base::RunLoop run_loop;
  watch_sink_id_ = std::move(sink_id);
  watch_callback_ = run_loop.QuitClosure();
  watch_type_ = watch_type;

  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);
  dialog_view->AddObserver(this);
  // Check if the current dialog state already meets the condition that we are
  // waiting for.
  OnDialogModelUpdated(dialog_view);

  run_loop.Run();
}

}  // namespace media_router

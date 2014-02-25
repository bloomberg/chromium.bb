// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/idle_app_name_notification_view.h"

#include <string>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ui {
class LayerAnimationSequence;
}

namespace chromeos {
namespace {

// Color of the text of the warning message.
const SkColor kTextColor = SK_ColorBLACK;

// Color of the text of the warning message.
const SkColor kErrorTextColor = SK_ColorRED;

// Color of the window background.
const SkColor kWindowBackgroundColor = SK_ColorWHITE;

// Radius of the rounded corners of the window.
const int kWindowCornerRadius = 4;

// Spacing around the text.
const int kHorizontalMarginAroundText = 50;
const int kVerticalMarginAroundText = 25;
const int kVerticalSpacingBetweenText = 10;

// Creates and shows the message widget for |view| with |animation_time_ms|.
void CreateAndShowWidgetWithContent(views::WidgetDelegate* delegate,
                                    views::View* view,
                                    int animation_time_ms) {
  aura::Window* root_window = ash::Shell::GetTargetRootWindow();
  gfx::Size rs = root_window->bounds().size();
  gfx::Size ps = view->GetPreferredSize();
  gfx::Rect bounds((rs.width() - ps.width()) / 2,
                   -ps.height(),
                   ps.width(),
                   ps.height());
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.accept_events = false;
  params.can_activate = false;
  params.keep_on_top = true;
  params.remove_standard_frame = true;
  params.delegate = delegate;
  params.bounds = bounds;
  params.parent = ash::Shell::GetContainer(
      root_window,
      ash::internal::kShellWindowId_SettingBubbleContainer);
  views::Widget* widget = new views::Widget;
  widget->Init(params);
  widget->SetContentsView(view);
  gfx::NativeView native_view = widget->GetNativeView();
  native_view->SetName("KioskIdleAppNameNotification");

  // Note: We cannot use the Window show/hide animations since they are disabled
  // for kiosk by command line.
  ui::LayerAnimator* animator = new ui::LayerAnimator(
          base::TimeDelta::FromMilliseconds(animation_time_ms));
  native_view->layer()->SetAnimator(animator);
  widget->Show();

  // We don't care about the show animation since it is off screen, so stop the
  // started animation and move the message into view.
  animator->StopAnimating();
  bounds.set_y((rs.height() - ps.height()) / 20);
  widget->SetBounds(bounds);

  // Allow to use the message for spoken feedback.
  view->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
}

}  // namespace

// The class which implements the content view for the message.
class IdleAppNameNotificationDelegateView
    : public views::WidgetDelegateView,
      public ui::ImplicitAnimationObserver {
 public:
  // An idle message which will get shown from the caller and hides itself after
  // a time, calling |owner->CloseMessage| to inform the owner that it got
  // destroyed. The |app_name| and the |app_author| are strings which get
  // used as message and |error| is true if something is not correct.
  // |message_visibility_time_in_ms| ms's after creation the message will start
  // to remove itself from the screen.
  IdleAppNameNotificationDelegateView(IdleAppNameNotificationView *owner,
                                      const base::string16& app_name,
                                      const base::string16& app_author,
                                      bool error,
                                      int message_visibility_time_in_ms)
      : owner_(owner),
        widget_closed_(false) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    // Add the application name label to the message.
    AddLabel(app_name,
             rb.GetFontList(ui::ResourceBundle::BoldFont),
             error && app_author.empty() ? kErrorTextColor : kTextColor);
    if (!app_author.empty()) {
      // Add the author label to the message.
      base::string16 app_by_author =
            l10n_util::GetStringFUTF16(IDS_IDLE_APP_NAME_NOTIFICATION,
                                       app_author);
      AddLabel(app_by_author,
               rb.GetFontList(ui::ResourceBundle::BaseFont),
               error ? kErrorTextColor : kTextColor);
      spoken_text_ =
          l10n_util::GetStringFUTF16(IDS_IDLE_APP_NAME_SPOKEN_NOTIFICATION,
                                     app_name,
                                     app_by_author);
      SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                            kHorizontalMarginAroundText,
                                            kVerticalMarginAroundText,
                                            kVerticalSpacingBetweenText));
    } else {
      spoken_text_ = app_name;
      SetLayoutManager(new views::FillLayout);
    }

    // Set a timer which will trigger to remove the message after the given
    // time.
    hide_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(message_visibility_time_in_ms),
        this,
        &IdleAppNameNotificationDelegateView::RemoveMessage);
  }

  virtual ~IdleAppNameNotificationDelegateView() {
    // The widget is already closing, but the other cleanup items need to be
    // performed.
    widget_closed_ = true;
    Close();
  }

  // Close the widget immediately. This can be called from the owner or from
  // this class.
  void Close() {
    // Stop the timer (if it was running).
    hide_timer_.Stop();
    // Inform our owner that we are going away.
    if (owner_) {
      IdleAppNameNotificationView* owner = owner_;
      owner_ = NULL;
      owner->CloseMessage();
    }
    // Close the owning widget - if required.
    if (!widget_closed_) {
      widget_closed_ = true;
      GetWidget()->Close();
    }
  }

  // Animate the window away (and close once done).
  void RemoveMessage() {
    aura::Window* widget_view = GetWidget()->GetNativeView();
    ui::Layer* layer = widget_view->layer();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.AddObserver(this);
    gfx::Rect rect = widget_view->bounds();
    rect.set_y(-GetPreferredSize().height());
    layer->SetBounds(rect);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(kWindowBackgroundColor);
    canvas->DrawRoundRect(GetLocalBounds(), kWindowCornerRadius, paint);
    views::WidgetDelegateView::OnPaint(canvas);
  }

  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE {
    state->name = spoken_text_;
    state->role = ui::AX_ROLE_ALERT;
  }

  // ImplicitAnimationObserver overrides
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    Close();
  }

 private:
  // Adds the label to the view, using |text| with a |font| and a |text_color|.
  void AddLabel(const base::string16& text,
                const gfx::FontList& font,
                SkColor text_color) {
    views::Label* label = new views::Label;
    label->SetText(text);
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label->SetFontList(font);
    label->SetEnabledColor(text_color);
    label->SetDisabledColor(text_color);
    label->SetAutoColorReadabilityEnabled(false);
    AddChildView(label);
  }

  // A timer which calls us to remove the message from the screen.
  base::OneShotTimer<IdleAppNameNotificationDelegateView> hide_timer_;

  // The owner of this message which needs to get notified when the message
  // closes.
  IdleAppNameNotificationView* owner_;

  // The spoken text.
  base::string16 spoken_text_;

  // True if the widget got already closed.
  bool widget_closed_;

  DISALLOW_COPY_AND_ASSIGN(IdleAppNameNotificationDelegateView);
};

IdleAppNameNotificationView::IdleAppNameNotificationView(
    int message_visibility_time_in_ms,
    int animation_time_ms,
    const extensions::Extension* extension)
    : view_(NULL) {
  ShowMessage(message_visibility_time_in_ms, animation_time_ms, extension);
}

IdleAppNameNotificationView::~IdleAppNameNotificationView() {
  CloseMessage();
}

void IdleAppNameNotificationView::CloseMessage() {
  if (view_) {
    IdleAppNameNotificationDelegateView* view = view_;
    view_ = NULL;
    view->Close();
  }
}

bool IdleAppNameNotificationView::IsVisible() {
  return view_ != NULL;
}

base::string16 IdleAppNameNotificationView::GetShownTextForTest() {
  ui::AXViewState state;
  DCHECK(view_);
  view_->GetAccessibleState(&state);
  return state.name;
}

void IdleAppNameNotificationView::ShowMessage(
    int message_visibility_time_in_ms,
    int animation_time_ms,
    const extensions::Extension* extension) {
  DCHECK(!view_);

  base::string16 author;
  base::string16 app_name;
  bool error = false;
  if (extension && !ContainsOnlyWhitespaceASCII(extension->name())) {
    app_name = base::UTF8ToUTF16(extension->name());
    // TODO(skuhne): This might not be enough since the author flag is not
    // explicitly enforced by us, but for Kiosk mode we could maybe require it.
    extension->manifest()->GetString("author", &author);
    if (ContainsOnlyWhitespace(author)) {
      error = true;
      author = l10n_util::GetStringUTF16(
          IDS_IDLE_APP_NAME_INVALID_AUTHOR_NOTIFICATION);
    }
  } else {
    error = true;
    app_name = l10n_util::GetStringUTF16(
        IDS_IDLE_APP_NAME_UNKNOWN_APPLICATION_NOTIFICATION);
  }

  view_ = new IdleAppNameNotificationDelegateView(
      this,
      app_name,
      author,
      error,
      message_visibility_time_in_ms + animation_time_ms);
  CreateAndShowWidgetWithContent(view_, view_, animation_time_ms);
}

}  // namespace chromeos

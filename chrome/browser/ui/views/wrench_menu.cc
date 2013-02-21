// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/wrench_menu.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/native_theme/native_theme_aura.h"
#endif

using content::HostZoomMap;
using content::UserMetricsAction;
using content::WebContents;
using ui::MenuModel;
using views::CustomButton;
using views::ImageButton;
using views::Label;
using views::MenuConfig;
using views::MenuItemView;
using views::TextButton;
using views::View;

namespace {

// Colors used for buttons.
const SkColor kHotBorderColor = SkColorSetARGB(72, 0, 0, 0);
const SkColor kBorderColor = SkColorSetARGB(36, 0, 0, 0);
const SkColor kPushedBorderColor = SkColorSetARGB(72, 0, 0, 0);
const SkColor kHotBackgroundColor = SkColorSetARGB(204, 255, 255, 255);
const SkColor kBackgroundColor = SkColorSetARGB(102, 255, 255, 255);
const SkColor kPushedBackgroundColor = SkColorSetARGB(13, 0, 0, 0);
const SkColor kTouchBackgroundColor = SkColorSetARGB(247, 255, 255, 255);
const SkColor kHotTouchBackgroundColor = SkColorSetARGB(247, 242, 242, 242);
const SkColor kPushedTouchBackgroundColor = SkColorSetARGB(247, 235, 235, 235);

const SkColor kTouchButtonText = 0xff5a5a5a;

// Horizontal padding on the edges of the buttons.
const int kHorizontalPadding = 6;
// Horizontal padding for a touch enabled menu.
const int kHorizontalTouchPadding = 15;

// Menu items which have embedded buttons should have this height in pixel.
const int kMenuItemContainingButtonsHeight = 43;

// Subclass of ImageButton whose preferred size includes the size of the border.
class FullscreenButton : public ImageButton {
 public:
  explicit FullscreenButton(views::ButtonListener* listener)
      : ImageButton(listener) { }

  // Overridden from ImageButton.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size pref = ImageButton::GetPreferredSize();
    if (border()) {
      gfx::Insets insets = border()->GetInsets();
      pref.Enlarge(insets.width(), insets.height());
    }
    return pref;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FullscreenButton);
};

// Border for buttons contained in the menu. This is only used for getting the
// insets, the actual painting is done in MenuButtonBackground.
class MenuButtonBorder : public views::Border {
 public:
  MenuButtonBorder(const MenuConfig& config, bool use_new_menu)
      : horizontal_padding_(use_new_menu ?
                            kHorizontalTouchPadding : kHorizontalPadding),
        insets_(config.item_top_margin, horizontal_padding_,
                config.item_bottom_margin, horizontal_padding_) {
  }

  // Overridden from views::Border.
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE {
    // Painting of border is done in MenuButtonBackground.
  }

  virtual gfx::Insets GetInsets() const OVERRIDE {
    return insets_;
  }

 private:
  // The horizontal padding dependent on the layout.
  const int horizontal_padding_;

  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(MenuButtonBorder);
};

// Combination border/background for the buttons contained in the menu. The
// painting of the border/background is done here as TextButton does not always
// paint the border.
class MenuButtonBackground : public views::Background {
 public:
  enum ButtonType {
    LEFT_BUTTON,
    CENTER_BUTTON,
    RIGHT_BUTTON,
    SINGLE_BUTTON,
  };

  MenuButtonBackground(ButtonType type, bool use_new_menu)
      : type_(type),
        use_new_menu_(use_new_menu),
        left_button_(NULL),
        right_button_(NULL) {}

  // Used when the type is CENTER_BUTTON to determine if the left/right edge
  // needs to be rendered selected.
  void SetOtherButtons(CustomButton* left_button, CustomButton* right_button) {
    if (base::i18n::IsRTL()) {
      left_button_ = right_button;
      right_button_ = left_button;
    } else {
      left_button_ = left_button;
      right_button_ = right_button;
    }
  }

  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, View* view) const OVERRIDE {
    CustomButton::ButtonState state =
        (view->GetClassName() == views::Label::kViewClassName) ?
        CustomButton::STATE_NORMAL : static_cast<CustomButton*>(view)->state();
    int w = view->width();
    int h = view->height();
#if defined(USE_AURA)
    if (use_new_menu_ &&
        view->GetNativeTheme() == ui::NativeThemeAura::instance()) {
      // Normal buttons get a border drawn on the right side and the rest gets
      // filled in. The left button however does not get a line to combine
      // buttons.
      int border = 0;
      if (type_ != RIGHT_BUTTON) {
        border = 1;
        canvas->FillRect(gfx::Rect(0, 0, border, h),
                         border_color(CustomButton::STATE_NORMAL));
      }
      canvas->FillRect(gfx::Rect(border, 0, w - border, h),
                       touch_background_color(state));
      return;
    }
#endif
    switch (TypeAdjustedForRTL()) {
      case LEFT_BUTTON:
        canvas->FillRect(gfx::Rect(1, 1, w, h - 2), background_color(state));
        canvas->FillRect(gfx::Rect(2, 0, w, 1), border_color(state));
        canvas->FillRect(gfx::Rect(1, 1, 1, 1), border_color(state));
        canvas->FillRect(gfx::Rect(0, 2, 1, h - 4), border_color(state));
        canvas->FillRect(gfx::Rect(1, h - 2, 1, 1), border_color(state));
        canvas->FillRect(gfx::Rect(2, h - 1, w, 1), border_color(state));
        break;

      case CENTER_BUTTON: {
        canvas->FillRect(gfx::Rect(1, 1, w - 2, h - 2),
                         background_color(state));
        SkColor left_color = state != CustomButton::STATE_NORMAL ?
            border_color(state) : border_color(left_button_->state());
        canvas->FillRect(gfx::Rect(0, 0, 1, h), left_color);
        canvas->FillRect(gfx::Rect(1, 0, w - 2, 1), border_color(state));
        canvas->FillRect(gfx::Rect(1, h - 1, w - 2, 1), border_color(state));
        SkColor right_color = state != CustomButton::STATE_NORMAL ?
            border_color(state) : border_color(right_button_->state());
        canvas->FillRect(gfx::Rect(w - 1, 0, 1, h), right_color);
        break;
      }

      case RIGHT_BUTTON:
        canvas->FillRect(gfx::Rect(0, 1, w - 1, h - 2),
                         background_color(state));
        canvas->FillRect(gfx::Rect(0, 0, w - 2, 1), border_color(state));
        canvas->FillRect(gfx::Rect(w - 2, 1, 1, 1), border_color(state));
        canvas->FillRect(gfx::Rect(w - 1, 2, 1, h - 4), border_color(state));
        canvas->FillRect(gfx::Rect(w - 2, h - 2, 1, 1), border_color(state));
        canvas->FillRect(gfx::Rect(0, h - 1, w - 2, 1), border_color(state));
        break;

      case SINGLE_BUTTON:
        canvas->FillRect(gfx::Rect(1, 1, w - 2, h - 2),
                         background_color(state));
        canvas->FillRect(gfx::Rect(2, 0, w - 4, 1), border_color(state));
        canvas->FillRect(gfx::Rect(1, 1, 1, 1), border_color(state));
        canvas->FillRect(gfx::Rect(0, 2, 1, h - 4), border_color(state));
        canvas->FillRect(gfx::Rect(1, h - 2, 1, 1), border_color(state));
        canvas->FillRect(gfx::Rect(2, h - 1, w - 4, 1), border_color(state));
        canvas->FillRect(gfx::Rect(w - 2, 1, 1, 1), border_color(state));
        canvas->FillRect(gfx::Rect(w - 1, 2, 1, h - 4), border_color(state));
        canvas->FillRect(gfx::Rect(w - 2, h - 2, 1, 1), border_color(state));
        break;

      default:
        NOTREACHED();
        break;
    }
  }

 private:
  static SkColor border_color(CustomButton::ButtonState state) {
    switch (state) {
      case CustomButton::STATE_HOVERED: return kHotBorderColor;
      case CustomButton::STATE_PRESSED: return kPushedBorderColor;
      default:                          return kBorderColor;
    }
  }

  static SkColor background_color(CustomButton::ButtonState state) {
    switch (state) {
      case CustomButton::STATE_HOVERED: return kHotBackgroundColor;
      case CustomButton::STATE_PRESSED: return kPushedBackgroundColor;
      default:                          return kBackgroundColor;
    }
  }

  static SkColor touch_background_color(CustomButton::ButtonState state) {
    switch (state) {
      case CustomButton::STATE_HOVERED: return kHotTouchBackgroundColor;
      case CustomButton::STATE_PRESSED: return kPushedTouchBackgroundColor;
      default:                          return kTouchBackgroundColor;
    }
  }

  ButtonType TypeAdjustedForRTL() const {
    if (!base::i18n::IsRTL())
      return type_;

    switch (type_) {
      case LEFT_BUTTON:   return RIGHT_BUTTON;
      case RIGHT_BUTTON:  return LEFT_BUTTON;
      default:            break;
    }
    return type_;
  }

  const ButtonType type_;
  const bool use_new_menu_;

  // See description above setter for details.
  CustomButton* left_button_;
  CustomButton* right_button_;

  DISALLOW_COPY_AND_ASSIGN(MenuButtonBackground);
};

string16 GetAccessibleNameForWrenchMenuItem(
      MenuModel* model, int item_index, int accessible_string_id) {
  string16 accessible_name = l10n_util::GetStringUTF16(accessible_string_id);
  string16 accelerator_text;

  ui::Accelerator menu_accelerator;
  if (model->GetAcceleratorAt(item_index, &menu_accelerator)) {
    accelerator_text =
        ui::Accelerator(menu_accelerator.key_code(),
                        menu_accelerator.modifiers()).GetShortcutText();
  }

  return MenuItemView::GetAccessibleNameForMenuItem(
      accessible_name, accelerator_text);
}

// WrenchMenuView is a view that can contain text buttons.
class WrenchMenuView : public views::View,
                       public views::ButtonListener {
 public:
  WrenchMenuView(WrenchMenu* menu, MenuModel* menu_model)
      : menu_(menu),
        menu_model_(menu_model) {}

  // Overridden from views::View.
  virtual void SchedulePaintInRect(const gfx::Rect& r) OVERRIDE {
    // Normally when the mouse enters/exits a button the buttons invokes
    // SchedulePaint. As part of the button border (MenuButtonBackground) is
    // rendered by the button to the left/right of it SchedulePaint on the the
    // button may not be enough, so this forces a paint all.
    View::SchedulePaintInRect(gfx::Rect(size()));
  }

  TextButton* CreateAndConfigureButton(int string_id,
                                       MenuButtonBackground::ButtonType type,
                                       int index,
                                       MenuButtonBackground** background) {
    return CreateButtonWithAccName(
      string_id, type, index, background, string_id);
  }

  TextButton* CreateButtonWithAccName(int string_id,
                                      MenuButtonBackground::ButtonType type,
                                      int index,
                                      MenuButtonBackground** background,
                                      int acc_string_id) {
    TextButton* button = new TextButton(this, gfx::RemoveAcceleratorChar(
        l10n_util::GetStringUTF16(string_id), '&', NULL, NULL));
    button->SetAccessibleName(
        GetAccessibleNameForWrenchMenuItem(menu_model_, index, acc_string_id));
    button->set_focusable(true);
    button->set_request_focus_on_press(false);
    button->set_tag(index);
    button->SetEnabled(menu_model_->IsEnabledAt(index));
    MenuButtonBackground* bg =
        new MenuButtonBackground(type, menu_->use_new_menu());
    button->set_background(bg);
    const MenuConfig& menu_config = menu_->GetMenuConfig();
    button->SetEnabledColor(menu_config.text_color);
    if (background)
      *background = bg;
    button->set_border(
        new MenuButtonBorder(menu_config, menu_->use_new_menu()));
    button->set_alignment(TextButton::ALIGN_CENTER);
    button->SetFont(menu_config.font);
    button->ClearMaxTextSize();
    AddChildView(button);
    return button;
  }

 protected:
  // Hosting WrenchMenu.
  WrenchMenu* menu_;

  // The menu model containing the increment/decrement/reset items.
  MenuModel* menu_model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WrenchMenuView);
};

class ButtonContainerMenuItemView : public MenuItemView {
 public:
  // Constructor for use with button containing menu items which have a
  // different height then normal items.
  ButtonContainerMenuItemView(MenuItemView* parent, int id, int height)
      : MenuItemView(parent, id, MenuItemView::NORMAL),
        height_(height) {
  };

  // Overridden from MenuItemView.
  virtual gfx::Size GetChildPreferredSize() OVERRIDE {
    gfx::Size size = MenuItemView::GetChildPreferredSize();
    // When there is a height override given, we need to deduct our spacing
    // above and below to get to the correct height to return here for the
    // child item.
    int height = height_ - GetTopMargin() - GetBottomMargin();
    if (height > size.height())
      size.set_height(height);
    return size;
  }

 private:
  int height_;

  DISALLOW_COPY_AND_ASSIGN(ButtonContainerMenuItemView);
};

}  // namespace

// CutCopyPasteView ------------------------------------------------------------

// CutCopyPasteView is the view containing the cut/copy/paste buttons.
class WrenchMenu::CutCopyPasteView : public WrenchMenuView {
 public:
  CutCopyPasteView(WrenchMenu* menu,
                   MenuModel* menu_model,
                   int cut_index,
                   int copy_index,
                   int paste_index)
      : WrenchMenuView(menu, menu_model) {
    TextButton* cut = CreateAndConfigureButton(
        IDS_CUT, MenuButtonBackground::LEFT_BUTTON, cut_index, NULL);

    MenuButtonBackground* copy_background = NULL;
    TextButton* copy = CreateAndConfigureButton(
        IDS_COPY, MenuButtonBackground::CENTER_BUTTON, copy_index,
        &copy_background);

    TextButton* paste = CreateAndConfigureButton(
        IDS_PASTE,
        menu_->use_new_menu() && menu_->supports_new_separators_ ?
            MenuButtonBackground::CENTER_BUTTON :
            MenuButtonBackground::RIGHT_BUTTON,
        paste_index,
        NULL);
    if (menu_->use_new_menu()) {
      cut->SetEnabledColor(kTouchButtonText);
      copy->SetEnabledColor(kTouchButtonText);
      paste->SetEnabledColor(kTouchButtonText);
    } else {
      SkColor text_color = GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor);
      cut->SetEnabledColor(text_color);
      copy->SetEnabledColor(text_color);
      paste->SetEnabledColor(text_color);
    }
    copy_background->SetOtherButtons(cut, paste);
  }

  // Overridden from View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // Returned height doesn't matter as MenuItemView forces everything to the
    // height of the menuitemview.
    return gfx::Size(GetMaxChildViewPreferredWidth() * child_count(), 0);
  }

  virtual void Layout() OVERRIDE {
    // All buttons are given the same width.
    int width = GetMaxChildViewPreferredWidth();
    for (int i = 0; i < child_count(); ++i)
      child_at(i)->SetBounds(i * width, 0, width, height());
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    menu_->CancelAndEvaluate(menu_model_, sender->tag());
  }

 private:
  // Returns the max preferred width of all the children.
  int GetMaxChildViewPreferredWidth() {
    int width = 0;
    for (int i = 0; i < child_count(); ++i)
      width = std::max(width, child_at(i)->GetPreferredSize().width());
    return width;
  }

  DISALLOW_COPY_AND_ASSIGN(CutCopyPasteView);
};

// ZoomView --------------------------------------------------------------------

// Padding between the increment buttons and the reset button.
static const int kZoomPadding = 6;
static const int kTouchZoomPadding = 14;

// ZoomView contains the various zoom controls: two buttons to increase/decrease
// the zoom, a label showing the current zoom percent, and a button to go
// full-screen.
class WrenchMenu::ZoomView : public WrenchMenuView {
 public:
  ZoomView(WrenchMenu* menu,
           MenuModel* menu_model,
           int decrement_index,
           int increment_index,
           int fullscreen_index)
      : WrenchMenuView(menu, menu_model),
        fullscreen_index_(fullscreen_index),
        zoom_callback_(base::Bind(&WrenchMenu::ZoomView::OnZoomLevelChanged,
                                  base::Unretained(this))),
        increment_button_(NULL),
        zoom_label_(NULL),
        decrement_button_(NULL),
        fullscreen_button_(NULL),
        zoom_label_width_(0) {
    HostZoomMap::GetForBrowserContext(
        menu_->browser_->profile())->AddZoomLevelChangedCallback(
            zoom_callback_);

    decrement_button_ = CreateButtonWithAccName(
        IDS_ZOOM_MINUS2, MenuButtonBackground::LEFT_BUTTON, decrement_index,
        NULL, IDS_ACCNAME_ZOOM_MINUS2);

    zoom_label_ = new Label(
        l10n_util::GetStringFUTF16Int(IDS_ZOOM_PERCENT, 100));
    zoom_label_->SetAutoColorReadabilityEnabled(false);
    zoom_label_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

    MenuButtonBackground* center_bg = new MenuButtonBackground(
        menu_->use_new_menu() && menu_->supports_new_separators_ ?
            MenuButtonBackground::RIGHT_BUTTON :
            MenuButtonBackground::CENTER_BUTTON,
        menu_->use_new_menu());
    zoom_label_->set_background(center_bg);
    const MenuConfig& menu_config(menu->GetMenuConfig());
    zoom_label_->set_border(
        new MenuButtonBorder(menu_config, menu->use_new_menu()));
    zoom_label_->SetFont(menu_config.font);

    AddChildView(zoom_label_);
    zoom_label_width_ = MaxWidthForZoomLabel();

    increment_button_ = CreateButtonWithAccName(
        IDS_ZOOM_PLUS2, MenuButtonBackground::RIGHT_BUTTON, increment_index,
        NULL, IDS_ACCNAME_ZOOM_PLUS2);

    center_bg->SetOtherButtons(decrement_button_, increment_button_);

    fullscreen_button_ = new FullscreenButton(this);
    gfx::ImageSkia* full_screen_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_FULLSCREEN_MENU_BUTTON);
    fullscreen_button_->SetImage(ImageButton::STATE_NORMAL, full_screen_image);
    if (menu_->use_new_menu()) {
      zoom_label_->SetEnabledColor(kTouchButtonText);
      decrement_button_->SetEnabledColor(kTouchButtonText);
      increment_button_->SetEnabledColor(kTouchButtonText);
    } else {
      SkColor enabled_text_color = GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor);
      zoom_label_->SetEnabledColor(enabled_text_color);
      decrement_button_->SetEnabledColor(enabled_text_color);
      increment_button_->SetEnabledColor(enabled_text_color);
      SkColor disabled_text_color = GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor);
      decrement_button_->SetDisabledColor(disabled_text_color);
      increment_button_->SetDisabledColor(disabled_text_color);
    }

    fullscreen_button_->set_focusable(true);
    fullscreen_button_->set_request_focus_on_press(false);
    fullscreen_button_->set_tag(fullscreen_index);
    fullscreen_button_->SetImageAlignment(
        ImageButton::ALIGN_CENTER, ImageButton::ALIGN_MIDDLE);
    int horizontal_padding =
        menu_->use_new_menu() ? kHorizontalTouchPadding : kHorizontalPadding;
    fullscreen_button_->set_border(views::Border::CreateEmptyBorder(
        0, horizontal_padding, 0, horizontal_padding));
    fullscreen_button_->set_background(
        new MenuButtonBackground(MenuButtonBackground::SINGLE_BUTTON,
                                 menu_->use_new_menu()));
    fullscreen_button_->SetAccessibleName(
        GetAccessibleNameForWrenchMenuItem(
            menu_model, fullscreen_index, IDS_ACCNAME_FULLSCREEN));
    AddChildView(fullscreen_button_);

    UpdateZoomControls();
  }

  virtual ~ZoomView() {
    HostZoomMap::GetForBrowserContext(
        menu_->browser_->profile())->RemoveZoomLevelChangedCallback(
            zoom_callback_);
  }

  // Overridden from View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // The increment/decrement button are forced to the same width.
    int button_width = std::max(increment_button_->GetPreferredSize().width(),
                                decrement_button_->GetPreferredSize().width());
    int zoom_padding = menu_->use_new_menu() ? kTouchZoomPadding : kZoomPadding;
    int fullscreen_width = fullscreen_button_->GetPreferredSize().width() +
                           zoom_padding;
    // Returned height doesn't matter as MenuItemView forces everything to the
    // height of the menuitemview. Note that we have overridden the height when
    // constructing the menu.
    return gfx::Size(button_width + zoom_label_width_ + button_width +
                     fullscreen_width, 0);
  }

  virtual void Layout() OVERRIDE {
    int x = 0;
    int button_width = std::max(increment_button_->GetPreferredSize().width(),
                                decrement_button_->GetPreferredSize().width());
    gfx::Rect bounds(0, 0, button_width, height());

    decrement_button_->SetBoundsRect(bounds);

    x += bounds.width();
    bounds.set_x(x);
    bounds.set_width(zoom_label_width_);
    zoom_label_->SetBoundsRect(bounds);

    x += bounds.width();
    bounds.set_x(x);
    bounds.set_width(button_width);
    increment_button_->SetBoundsRect(bounds);

    x += bounds.width() + (menu_->use_new_menu() ? 0 : kZoomPadding);
    bounds.set_x(x);
    bounds.set_width(fullscreen_button_->GetPreferredSize().width() +
                     (menu_->use_new_menu() ? kTouchZoomPadding : 0));
    fullscreen_button_->SetBoundsRect(bounds);
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender->tag() == fullscreen_index_) {
      menu_->CancelAndEvaluate(menu_model_, sender->tag());
    } else {
      // Zoom buttons don't close the menu.
      menu_model_->ActivatedAt(sender->tag());
    }
  }

 private:
  void OnZoomLevelChanged(const std::string& host) {
    UpdateZoomControls();
  }

  void UpdateZoomControls() {
    bool enable_increment = false;
    bool enable_decrement = false;
    WebContents* selected_tab =
        menu_->browser_->tab_strip_model()->GetActiveWebContents();
    int zoom = 100;
    if (selected_tab)
      zoom = selected_tab->GetZoomPercent(&enable_increment, &enable_decrement);
    increment_button_->SetEnabled(enable_increment);
    decrement_button_->SetEnabled(enable_decrement);
    zoom_label_->SetText(
        l10n_util::GetStringFUTF16Int(IDS_ZOOM_PERCENT, zoom));

    zoom_label_width_ = MaxWidthForZoomLabel();
  }

  // Calculates the max width the zoom string can be.
  int MaxWidthForZoomLabel() {
    gfx::Font font = zoom_label_->font();
    int border_width =
        zoom_label_->border() ? zoom_label_->border()->GetInsets().width() : 0;

    int max_w = 0;

    WebContents* selected_tab =
        menu_->browser_->tab_strip_model()->GetActiveWebContents();
    if (selected_tab) {
      int min_percent = selected_tab->GetMinimumZoomPercent();
      int max_percent = selected_tab->GetMaximumZoomPercent();

      int step = (max_percent - min_percent) / 10;
      for (int i = min_percent; i <= max_percent; i += step) {
        int w = font.GetStringWidth(
            l10n_util::GetStringFUTF16Int(IDS_ZOOM_PERCENT, i));
        max_w = std::max(w, max_w);
      }
    } else {
      max_w = font.GetStringWidth(
          l10n_util::GetStringFUTF16Int(IDS_ZOOM_PERCENT, 100));
    }

    return max_w + border_width;
  }

  // Index of the fullscreen menu item in the model.
  const int fullscreen_index_;

  content::HostZoomMap::ZoomLevelChangedCallback zoom_callback_;
  content::NotificationRegistrar registrar_;

  // Button for incrementing the zoom.
  TextButton* increment_button_;

  // Label showing zoom as a percent.
  Label* zoom_label_;

  // Button for decrementing the zoom.
  TextButton* decrement_button_;

  ImageButton* fullscreen_button_;

  // Width given to |zoom_label_|. This is the width at 100%.
  int zoom_label_width_;

  DISALLOW_COPY_AND_ASSIGN(ZoomView);
};

// RecentTabsMenuModelDelegate -------------------------------------------------

// Provides the ui::MenuModelDelegate implementation for RecentTabsSubMenuModel
// items.
class WrenchMenu::RecentTabsMenuModelDelegate : public ui::MenuModelDelegate {
 public:
  RecentTabsMenuModelDelegate(ui::MenuModel* model,
                              views::MenuItemView* menu_item)
      : model_(model),
        menu_item_(menu_item) {
    model_->SetMenuModelDelegate(this);
  }

  virtual ~RecentTabsMenuModelDelegate() {
    model_->SetMenuModelDelegate(NULL);
  }

  // ui::MenuModelDelegate implementation:
  virtual void OnIconChanged(int index) OVERRIDE {
    // |index| specifies position in children items of |menu_item_| starting at
    // 0, its corresponding command id as used in the children menu item views
    // follows that of the parent menu item view |menu_item_|.
    int command_id = menu_item_->GetCommand() + 1 + index;
    views::MenuItemView* item = menu_item_->GetMenuItemByID(command_id);
    DCHECK(item);
    gfx::Image icon;
    if (model_->GetIconAt(index, &icon))
      item->SetIcon(*icon.ToImageSkia());
  }

  // Return the specific menu width of recent tab menu item if |command_id|
  // refers to one of recent tabs menu items, else return -1.
  int GetMaxWidthForMenu(MenuItemView* menu) {
    views::SubmenuView* submenu = menu_item_->GetSubmenu();
    if (!submenu)
      return -1;
    const int kMaxMenuItemWidth = 320;
    return menu->GetCommand() >= menu_item_->GetCommand() &&
        menu->GetCommand() <=
            menu_item_->GetCommand() + submenu->GetMenuItemCount() ?
        kMaxMenuItemWidth : -1;
  }

 private:
  ui::MenuModel* model_;
  views::MenuItemView* menu_item_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabsMenuModelDelegate);
};

// WrenchMenu ------------------------------------------------------------------

WrenchMenu::WrenchMenu(Browser* browser,
                       bool use_new_menu,
                       bool supports_new_separators)
    : root_(NULL),
      browser_(browser),
      selected_menu_model_(NULL),
      selected_index_(0),
      bookmark_menu_(NULL),
      feedback_menu_item_(NULL),
      first_bookmark_command_id_(0),
      use_new_menu_(use_new_menu),
      supports_new_separators_(supports_new_separators) {
  registrar_.Add(this, chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
                 content::Source<Profile>(browser_->profile()));
}

WrenchMenu::~WrenchMenu() {
  if (bookmark_menu_delegate_.get()) {
    BookmarkModel* model = BookmarkModelFactory::GetForProfile(
        browser_->profile());
    if (model)
      model->RemoveObserver(this);
  }
}

void WrenchMenu::Init(ui::MenuModel* model) {
  DCHECK(!root_);
  root_ = new MenuItemView(this);
  root_->set_has_icons(true);  // We have checks, radios and icons, set this
                               // so we get the taller menu style.
  int next_id = 1;
  PopulateMenu(root_, model, &next_id);
  first_bookmark_command_id_ = next_id + 1;
  menu_runner_.reset(new views::MenuRunner(root_));
}

void WrenchMenu::RunMenu(views::MenuButton* host) {
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(host, &screen_loc);
  gfx::Rect bounds(screen_loc, host->size());
  content::RecordAction(UserMetricsAction("ShowAppMenu"));
  if (menu_runner_->RunMenuAt(host->GetWidget(), host, bounds,
          MenuItemView::TOPRIGHT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
  if (bookmark_menu_delegate_.get()) {
    BookmarkModel* model = BookmarkModelFactory::GetForProfile(
        browser_->profile());
    if (model)
      model->RemoveObserver(this);
  }
  if (selected_menu_model_)
    selected_menu_model_->ActivatedAt(selected_index_);
}

bool WrenchMenu::IsShowing() {
  return menu_runner_.get() && menu_runner_->IsRunning();
}

const views::MenuConfig& WrenchMenu::GetMenuConfig() const {
  views::Widget* browser_widget = views::Widget::GetWidgetForNativeView(
      browser_->window()->GetNativeWindow());
  DCHECK(browser_widget);
  return MenuConfig::instance(browser_widget->GetNativeTheme());
}

string16 WrenchMenu::GetTooltipText(int id,
                                    const gfx::Point& p) const {
  return is_bookmark_command(id) ?
      bookmark_menu_delegate_->GetTooltipText(id, p) : string16();
}

bool WrenchMenu::IsTriggerableEvent(views::MenuItemView* menu,
                                    const ui::Event& e) {
  return is_bookmark_command(menu->GetCommand()) ?
      bookmark_menu_delegate_->IsTriggerableEvent(menu, e) :
      MenuDelegate::IsTriggerableEvent(menu, e);
}

bool WrenchMenu::GetDropFormats(
      MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  CreateBookmarkMenu();
  return bookmark_menu_delegate_.get() &&
      bookmark_menu_delegate_->GetDropFormats(menu, formats, custom_formats);
}

bool WrenchMenu::AreDropTypesRequired(MenuItemView* menu) {
  CreateBookmarkMenu();
  return bookmark_menu_delegate_.get() &&
      bookmark_menu_delegate_->AreDropTypesRequired(menu);
}

bool WrenchMenu::CanDrop(MenuItemView* menu,
                         const ui::OSExchangeData& data) {
  CreateBookmarkMenu();
  return bookmark_menu_delegate_.get() &&
      bookmark_menu_delegate_->CanDrop(menu, data);
}

int WrenchMenu::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  return is_bookmark_command(item->GetCommand()) ?
      bookmark_menu_delegate_->GetDropOperation(item, event, position) :
      ui::DragDropTypes::DRAG_NONE;
}

int WrenchMenu::OnPerformDrop(MenuItemView* menu,
                              DropPosition position,
                              const ui::DropTargetEvent& event) {
  if (!is_bookmark_command(menu->GetCommand()))
    return ui::DragDropTypes::DRAG_NONE;

  int result = bookmark_menu_delegate_->OnPerformDrop(menu, position, event);
  return result;
}

bool WrenchMenu::ShowContextMenu(MenuItemView* source,
                                 int id,
                                 const gfx::Point& p,
                                 bool is_mouse_gesture) {
  return is_bookmark_command(id) ?
      bookmark_menu_delegate_->ShowContextMenu(source, id, p,
                                               is_mouse_gesture) :
      false;
}

bool WrenchMenu::CanDrag(MenuItemView* menu) {
  return is_bookmark_command(menu->GetCommand()) ?
      bookmark_menu_delegate_->CanDrag(menu) : false;
}

void WrenchMenu::WriteDragData(MenuItemView* sender,
                               ui::OSExchangeData* data) {
  DCHECK(is_bookmark_command(sender->GetCommand()));
  return bookmark_menu_delegate_->WriteDragData(sender, data);
}

int WrenchMenu::GetDragOperations(MenuItemView* sender) {
  return is_bookmark_command(sender->GetCommand()) ?
      bookmark_menu_delegate_->GetDragOperations(sender) :
      MenuDelegate::GetDragOperations(sender);
}

int WrenchMenu::GetMaxWidthForMenu(MenuItemView* menu) {
  if (is_bookmark_command(menu->GetCommand()))
    return bookmark_menu_delegate_->GetMaxWidthForMenu(menu);
  int max_width = -1;
  // If recent tabs menu is available, it will decide if |menu| is one of recent
  // tabs; if yes, it would return the menu width for recent tabs.
  // otherwise, it would return -1.
  if (recent_tabs_menu_model_delegate_.get())
    max_width = recent_tabs_menu_model_delegate_->GetMaxWidthForMenu(menu);
  if (max_width == -1)
    max_width = MenuDelegate::GetMaxWidthForMenu(menu);
  return max_width;
}

bool WrenchMenu::IsItemChecked(int id) const {
  if (is_bookmark_command(id))
    return false;

  const Entry& entry = id_to_entry_.find(id)->second;
  return entry.first->IsItemCheckedAt(entry.second);
}

bool WrenchMenu::IsCommandEnabled(int id) const {
  if (is_bookmark_command(id))
    return true;

  if (id == 0)
    return false;  // The root item.

  const Entry& entry = id_to_entry_.find(id)->second;
  int command_id = entry.first->GetCommandIdAt(entry.second);
  // The items representing the cut menu (cut/copy/paste) and zoom menu
  // (increment/decrement/reset) are always enabled. The child views of these
  // items enabled state updates appropriately.
  return command_id == IDC_CUT || command_id == IDC_ZOOM_MINUS ||
      entry.first->IsEnabledAt(entry.second);
}

void WrenchMenu::ExecuteCommand(int id, int mouse_event_flags) {
  if (is_bookmark_command(id)) {
    bookmark_menu_delegate_->ExecuteCommand(id, mouse_event_flags);
    return;
  }

  // Not a bookmark
  const Entry& entry = id_to_entry_.find(id)->second;
  int command_id = entry.first->GetCommandIdAt(entry.second);

  if (command_id == IDC_CUT || command_id == IDC_ZOOM_MINUS) {
    // These items are represented by child views. If ExecuteCommand is invoked
    // it means the user clicked on the area around the buttons and we should
    // not do anyting.
    return;
  }

  return entry.first->ActivatedAt(entry.second, mouse_event_flags);
}

bool WrenchMenu::GetAccelerator(int id, ui::Accelerator* accelerator) {
  if (is_bookmark_command(id))
    return false;
  IDToEntry::iterator ix = id_to_entry_.find(id);
  if (ix == id_to_entry_.end()) {
    // There is no entry for this id.
    return false;
  }

  const Entry& entry = ix->second;
  int command_id = entry.first->GetCommandIdAt(entry.second);
  if (command_id == IDC_CUT || command_id == IDC_ZOOM_MINUS) {
    // These have special child views; don't show the accelerator for them.
    return false;
  }

  ui::Accelerator menu_accelerator;
  if (!entry.first->GetAcceleratorAt(entry.second, &menu_accelerator))
    return false;

  *accelerator = ui::Accelerator(menu_accelerator.key_code(),
                                 menu_accelerator.modifiers());
  return true;
}

void WrenchMenu::WillShowMenu(MenuItemView* menu) {
  if (menu == bookmark_menu_)
    CreateBookmarkMenu();
}

void WrenchMenu::WillHideMenu(MenuItemView* menu) {
  // Turns off the fade out animation of the wrench menus if
  // |feedback_menu_item_| is selected.  This excludes the wrench menu itself
  // from the snapshot in the feedback UI.
  if (menu->HasSubmenu() && feedback_menu_item_ &&
      feedback_menu_item_->IsSelected()) {
    // It's okay to just turn off the animation and no to take care the
    // animation back because the menu widget will be recreated next time
    // it's opened. See ToolbarView::RunMenu() and Init() of this class.
    menu->GetSubmenu()->GetWidget()->
        SetVisibilityChangedAnimationsEnabled(false);
  }
}

void WrenchMenu::BookmarkModelChanged() {
  DCHECK(bookmark_menu_delegate_.get());
  if (!bookmark_menu_delegate_->is_mutating_model())
    root_->Cancel();
}

void WrenchMenu::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED:
      // A change in the global errors list can add or remove items from the
      // menu. Close the menu to avoid have a stale menu on-screen.
      root_->Cancel();
      break;
    default:
      NOTREACHED();
  }
}

void WrenchMenu::PopulateMenu(MenuItemView* parent,
                              MenuModel* model,
                              int* next_id) {
  for (int i = 0, max = model->GetItemCount(); i < max; ++i) {
    // The button container menu items have a special height which we have to
    // use instead of the normal height.
    int height = 0;
    if (use_new_menu_ &&
        (model->GetCommandIdAt(i) == IDC_CUT ||
         model->GetCommandIdAt(i) == IDC_ZOOM_MINUS))
      height = kMenuItemContainingButtonsHeight;

    MenuItemView* item = AppendMenuItem(
        parent, model, i, model->GetTypeAt(i), next_id, height);

    if (model->GetTypeAt(i) == MenuModel::TYPE_SUBMENU)
      PopulateMenu(item, model->GetSubmenuModelAt(i), next_id);

    switch (model->GetCommandIdAt(i)) {
      case IDC_CUT:
        DCHECK_EQ(MenuModel::TYPE_COMMAND, model->GetTypeAt(i));
        DCHECK_LT(i + 2, max);
        DCHECK_EQ(IDC_COPY, model->GetCommandIdAt(i + 1));
        DCHECK_EQ(IDC_PASTE, model->GetCommandIdAt(i + 2));
        item->SetTitle(l10n_util::GetStringUTF16(IDS_EDIT2));
        item->AddChildView(new CutCopyPasteView(this, model, i, i + 1, i + 2));
        i += 2;
        break;

      case IDC_ZOOM_MINUS:
        DCHECK_EQ(MenuModel::TYPE_COMMAND, model->GetTypeAt(i));
        DCHECK_EQ(IDC_ZOOM_PLUS, model->GetCommandIdAt(i + 1));
        DCHECK_EQ(IDC_FULLSCREEN, model->GetCommandIdAt(i + 2));
        item->SetTitle(l10n_util::GetStringUTF16(IDS_ZOOM_MENU2));
        item->AddChildView(new ZoomView(this, model, i, i + 1, i + 2));
        i += 2;
        break;

      case IDC_BOOKMARKS_MENU:
        DCHECK(!bookmark_menu_);
        bookmark_menu_ = item;
        break;

      case IDC_FEEDBACK:
        DCHECK(!feedback_menu_item_);
        feedback_menu_item_ = item;
        break;

      case IDC_RECENT_TABS_MENU:
        DCHECK(chrome::search::IsInstantExtendedAPIEnabled(
            browser_->profile()));
        DCHECK(!recent_tabs_menu_model_delegate_.get());
        recent_tabs_menu_model_delegate_.reset(
            new RecentTabsMenuModelDelegate(model->GetSubmenuModelAt(i),
                                            item));
        break;

      default:
        break;
    }
  }
}

MenuItemView* WrenchMenu::AppendMenuItem(MenuItemView* parent,
                                         MenuModel* model,
                                         int index,
                                         MenuModel::ItemType menu_type,
                                         int* next_id,
                                         int height) {
  int id = (*next_id)++;

  id_to_entry_[id].first = model;
  id_to_entry_[id].second = index;

  MenuItemView* menu_item = NULL;
  if (height > 0) {
    // For menu items with a special menu height we use our special class to be
    // able to modify the item height.
    menu_item = new ButtonContainerMenuItemView(parent, id, height);
    parent->GetSubmenu()->AddChildView(menu_item);
  } else {
    // For all other cases we use the more generic way to add menu items.
    menu_item = parent->AppendMenuItemFromModel(model, index, id);
  }

  if (menu_item) {
    // Flush all buttons to the right side of the menu for the new menu type.
    menu_item->set_use_right_margin(!use_new_menu_);
    menu_item->SetVisible(model->IsVisibleAt(index));

    if (menu_type == MenuModel::TYPE_COMMAND && model->HasIcons()) {
      gfx::Image icon;
      if (model->GetIconAt(index, &icon))
        menu_item->SetIcon(*icon.ToImageSkia());
    }
  }

  return menu_item;
}

void WrenchMenu::CancelAndEvaluate(MenuModel* model, int index) {
  selected_menu_model_ = model;
  selected_index_ = index;
  root_->Cancel();
}

void WrenchMenu::CreateBookmarkMenu() {
  if (bookmark_menu_delegate_.get())
    return;  // Already created the menu.

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(
      browser_->profile());
  if (!model->IsLoaded())
    return;

  model->AddObserver(this);

  // TODO(oshima): Replace with views only API.
  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  bookmark_menu_delegate_.reset(
      new BookmarkMenuDelegate(browser_,
                               browser_,
                               parent,
                               first_bookmark_command_id_));
  bookmark_menu_delegate_->Init(
      this, bookmark_menu_, model->bookmark_bar_node(), 0,
      BookmarkMenuDelegate::SHOW_PERMANENT_FOLDERS,
      bookmark_utils::LAUNCH_WRENCH_MENU);
}

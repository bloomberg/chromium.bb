// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_button.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/avatar_button_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/label_button_border.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/ui/views/frame/minimize_button_metrics_win.h"
#endif

#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
#include "chrome/browser/ui/views/nav_button_provider.h"
#endif

namespace {

constexpr int kGenericAvatarIconSize = 16;

// TODO(emx): Calculate width based on caption button [http://crbug.com/716365]
constexpr int kCondensibleButtonMinWidth = 46;
// TODO(emx): Should this be calculated based on average character width?
constexpr int kCondensibleButtonMaxWidth = 98;

#if defined(OS_WIN)
constexpr gfx::Insets kBorderInsets(2, 8, 4, 8);

std::unique_ptr<views::Border> CreateThemedBorder(
    const int normal_image_set[],
    const int hot_image_set[],
    const int pushed_image_set[]) {
  std::unique_ptr<views::LabelButtonAssetBorder> border(
      new views::LabelButtonAssetBorder(views::Button::STYLE_TEXTBUTTON));

  border->SetPainter(false, views::Button::STATE_NORMAL,
                     views::Painter::CreateImageGridPainter(normal_image_set));
  border->SetPainter(false, views::Button::STATE_HOVERED,
                     views::Painter::CreateImageGridPainter(hot_image_set));
  border->SetPainter(false, views::Button::STATE_PRESSED,
                     views::Painter::CreateImageGridPainter(pushed_image_set));

  border->set_insets(kBorderInsets);

  return std::move(border);
}
#endif

// This class draws the border (and background) of the avatar button for
// "themed" browser windows, i.e. OpaqueBrowserFrameView. Currently it's only
// used on Linux as the shape specifically matches the Linux caption buttons.
// TODO(estade): make this look nice on Windows and use it there as well.
class AvatarButtonThemedBorder : public views::Border {
 public:
  AvatarButtonThemedBorder() {}
  ~AvatarButtonThemedBorder() override {}

  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    // Fill the color/background image from the theme.
    cc::PaintFlags fill_flags;
    fill_flags.setAntiAlias(true);
    const ui::ThemeProvider* theme = view.GetThemeProvider();
    fill_flags.setColor(
        theme->GetColor(ThemeProperties::COLOR_BUTTON_BACKGROUND));
    SkPath fill_path;
    gfx::Rect fill_bounds = view.GetLocalBounds();
    // The fill should overlap the inner stroke but not the outer stroke. But we
    // don't inset the top because as it stands, the asset-based window controls
    // fill one pixel higher due to how the background masking works out. Not
    // matching that is very noticeable. TODO(estade): when the window
    // controls use this same code, inset all sides equally.
    fill_bounds.Inset(gfx::Insets(0, kStrokeWidth, kStrokeWidth, kStrokeWidth));
    fill_path.addRoundRect(gfx::RectToSkRect(fill_bounds), kCornerRadius,
                           kCornerRadius);
    canvas->DrawPath(fill_path, fill_flags);
    fill_flags.setColor(SK_ColorBLACK);
    canvas->DrawImageInPath(
        *theme->GetImageSkiaNamed(IDR_THEME_WINDOW_CONTROL_BACKGROUND), 0, 0,
        fill_path, fill_flags);

    // Paint an outer dark stroke.
    cc::PaintFlags stroke_flags;
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    // The colors are chosen to match the assets we use for Linux.
    stroke_flags.setColor(SkColorSetA(SK_ColorBLACK, 0x2B));
    stroke_flags.setStrokeWidth(kStrokeWidth);
    stroke_flags.setAntiAlias(true);
    gfx::RectF stroke_bounds(view.GetLocalBounds());
    stroke_bounds.Inset(gfx::InsetsF(0.5f));
    canvas->DrawRoundRect(stroke_bounds, kCornerRadius, stroke_flags);

    // There's a second, light stroke that matches the fill bounds.
    stroke_bounds.Inset(gfx::InsetsF(kStrokeWidth));
    stroke_flags.setColor(SkColorSetA(SK_ColorWHITE, 0x3F));
    canvas->DrawRoundRect(stroke_bounds, kCornerRadius, stroke_flags);
  }

  gfx::Insets GetInsets() const override {
    auto insets = views::LabelButtonAssetBorder::GetDefaultInsetsForStyle(
        views::Button::STYLE_TEXTBUTTON);
    return kBorderStrokeInsets +
           gfx::Insets(0, insets.left(), 0, insets.right());
  }

  gfx::Size GetMinimumSize() const override {
    return gfx::Size(GetInsets().width(), GetInsets().height());
  }

  static std::unique_ptr<views::InkDropMask> CreateInkDropMask(
      const gfx::Size& size) {
    return base::MakeUnique<views::RoundRectInkDropMask>(
        size, kBorderStrokeInsets, kCornerRadius);
  }

 private:
  static constexpr int kStrokeWidth = 1;

  // Insets between view bounds and the interior of the strokes.
  static constexpr gfx::Insets kBorderStrokeInsets{kStrokeWidth * 2};

  // Corner radius of the roundrect.
  static constexpr float kCornerRadius = 1;

  DISALLOW_COPY_AND_ASSIGN(AvatarButtonThemedBorder);
};

constexpr int AvatarButtonThemedBorder::kStrokeWidth;
constexpr gfx::Insets AvatarButtonThemedBorder::kBorderStrokeInsets;
constexpr float AvatarButtonThemedBorder::kCornerRadius;

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "AvatarButtonShutdownNotifierFactory") {
    DependsOn(SigninManagerFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

}  // namespace

AvatarButton::AvatarButton(views::MenuButtonListener* listener,
                           AvatarButtonStyle button_style,
                           Profile* profile,
                           AvatarButtonManager* manager)
    : MenuButton(base::string16(), listener, false),
      error_controller_(this, profile),
      profile_(profile),
      profile_observer_(this),
      button_style_(button_style),
      widget_observer_(this) {
#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
  views::NavButtonProvider* nav_button_provider =
      manager->get_nav_button_provider();
  render_native_nav_buttons_ = nav_button_provider != nullptr;
#endif
  set_notify_action(Button::NOTIFY_ON_PRESS);
  set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                              ui::EF_RIGHT_MOUSE_BUTTON);
  set_animate_on_state_change(false);
  SetEnabledTextColors(SK_ColorWHITE);
  SetTextSubpixelRenderingEnabled(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  profile_observer_.Add(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

  // The largest text height that fits in the button. If the font list height
  // is larger than this, it will be shrunk to match it.
  // TODO(noms): Calculate this constant algorithmically from the button's size.
  const int kDisplayFontHeight = 16;
  label()->SetFontList(
      label()->font_list().DeriveWithHeightUpperBound(kDisplayFontHeight));

  bool apply_ink_drop = IsCondensible();
#if defined(OS_LINUX)
  DCHECK_EQ(AvatarButtonStyle::THEMED, button_style);
  apply_ink_drop = true;
#endif
  if (render_native_nav_buttons_)
    apply_ink_drop = false;

  if (render_native_nav_buttons_) {
#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
    SetBackground(nav_button_provider->CreateAvatarButtonBackground(this));
    SetBorder(nullptr);
    generic_avatar_ =
        gfx::CreateVectorIcon(kProfileSwitcherOutlineIcon,
                              kGenericAvatarIconSize, gfx::kPlaceholderColor);
#endif
  } else if (apply_ink_drop) {
    SetInkDropMode(InkDropMode::ON);
    SetFocusPainter(nullptr);
#if defined(OS_LINUX)
    set_ink_drop_base_color(SK_ColorWHITE);
    SetBorder(base::MakeUnique<AvatarButtonThemedBorder>());
    generic_avatar_ =
        gfx::CreateVectorIcon(kProfileSwitcherOutlineIcon,
                              kGenericAvatarIconSize, gfx::kPlaceholderColor);
#elif defined(OS_WIN)
    DCHECK_EQ(AvatarButtonStyle::NATIVE, button_style);
    SetBorder(views::CreateEmptyBorder(kBorderInsets));
  } else if (button_style == AvatarButtonStyle::THEMED) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_NORMAL);
    const int kHoverImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_HOVER);
    const int kPressedImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_PRESSED);
    SetButtonAvatar(IDR_AVATAR_THEMED_BUTTON_AVATAR);
    SetBorder(
        CreateThemedBorder(kNormalImageSet, kHoverImageSet, kPressedImageSet));
  } else if (base::win::GetVersion() < base::win::VERSION_WIN8) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_NORMAL);
    const int kHoverImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_HOVER);
    const int kPressedImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_PRESSED);
    SetButtonAvatar(IDR_AVATAR_GLASS_BUTTON_AVATAR);
    SetBorder(
        CreateThemedBorder(kNormalImageSet, kHoverImageSet, kPressedImageSet));
  } else {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_NORMAL);
    const int kHoverImageSet[] = IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_HOVER);
    const int kPressedImageSet[] = IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_PRESSED);
    SetButtonAvatar(IDR_AVATAR_NATIVE_BUTTON_AVATAR);
    SetBorder(
        CreateThemedBorder(kNormalImageSet, kHoverImageSet, kPressedImageSet));
#endif
  }

  profile_shutdown_notifier_ =
      ShutdownNotifierFactory::GetInstance()->Get(profile_)->Subscribe(
          base::Bind(&AvatarButton::OnProfileShutdown, base::Unretained(this)));
}

AvatarButton::~AvatarButton() {}

void AvatarButton::SetupThemeColorButton() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  if (IsCondensible()) {
    // TODO(bsep): This needs to also be called when the Windows accent color
    // updates, but there is currently no signal for that.
    const SkColor base_color = color_utils::IsDark(GetThemeProvider()->GetColor(
                                   ThemeProperties::COLOR_FRAME))
                                   ? SK_ColorWHITE
                                   : SK_ColorBLACK;
    set_ink_drop_base_color(base_color);
    const SkColor icon_color =
        SkColorSetA(base_color, static_cast<SkAlpha>(0.54 * 0xFF));
    generic_avatar_ = gfx::CreateVectorIcon(kAccountCircleIcon,
                                            kGenericAvatarIconSize, icon_color);
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

void AvatarButton::OnAvatarButtonPressed(const ui::Event* event) {
  views::Widget* bubble_widget = ProfileChooserView::GetCurrentBubbleWidget();
  if (bubble_widget && !widget_observer_.IsObserving(bubble_widget)) {
    widget_observer_.Add(bubble_widget);
    pressed_lock_ = std::make_unique<PressedLock>(
        this, false, ui::LocatedEvent::FromIfValid(event));
  }
}

void AvatarButton::AddedToWidget() {
  SetupThemeColorButton();
  Update();
}

void AvatarButton::OnGestureEvent(ui::GestureEvent* event) {
  // TODO(wjmaclean): The check for ET_GESTURE_LONG_PRESS is done here since
  // no other UI button based on Button appears to handle mouse
  // right-click. If other cases are identified, it may make sense to move this
  // check to Button.
  if (event->type() == ui::ET_GESTURE_LONG_PRESS)
    NotifyClick(*event);
  else
    MenuButton::OnGestureEvent(event);
}

gfx::Size AvatarButton::GetMinimumSize() const {
  if (IsCondensible()) {
    // Returns the size of the button when it is atop the tabstrip. Called by
    // GlassBrowserFrameView::LayoutProfileSwitcher().
    // TODO(emx): Calculate the height based on the top of the new tab button.
    return gfx::Size(kCondensibleButtonMinWidth, 20);
  }

  return MenuButton::GetMinimumSize();
}

gfx::Size AvatarButton::CalculatePreferredSize() const {
  if (render_native_nav_buttons_)
    return MenuButton::CalculatePreferredSize();

  // TODO(estade): Calculate the height instead of hardcoding to 20 for the
  // not-condensible case.
  gfx::Size size(MenuButton::CalculatePreferredSize().width(), 20);

  if (IsCondensible()) {
    // Returns the normal size of the button (when it does not overlap the
    // tabstrip).
    size.set_width(std::min(std::max(size.width(), kCondensibleButtonMinWidth),
                            kCondensibleButtonMaxWidth));
#if defined(OS_WIN)
    size.set_height(MinimizeButtonMetrics::GetCaptionButtonHeightInDIPs());
#endif
  }

  return size;
}

std::unique_ptr<views::InkDropMask> AvatarButton::CreateInkDropMask() const {
  if (button_style_ == AvatarButtonStyle::THEMED)
    return AvatarButtonThemedBorder::CreateInkDropMask(size());
  return MenuButton::CreateInkDropMask();
}

std::unique_ptr<views::InkDropHighlight> AvatarButton::CreateInkDropHighlight()
    const {
  if (button_style_ == AvatarButtonStyle::THEMED)
    return MenuButton::CreateInkDropHighlight();

  auto ink_drop_highlight = base::MakeUnique<views::InkDropHighlight>(
      size(), 0, gfx::RectF(GetLocalBounds()).CenterPoint(),
      GetInkDropBaseColor());
  constexpr float kInkDropHighlightOpacity = 0.08f;
  ink_drop_highlight->set_visible_opacity(kInkDropHighlightOpacity);
  return ink_drop_highlight;
}

bool AvatarButton::ShouldEnterPushedState(const ui::Event& event) {
  if (ProfileChooserView::IsShowing())
    return false;

  return MenuButton::ShouldEnterPushedState(event);
}

bool AvatarButton::ShouldUseFloodFillInkDrop() const {
  return true;
}

void AvatarButton::OnAvatarErrorChanged() {
  Update();
}

void AvatarButton::OnProfileAdded(const base::FilePath& profile_path) {
  Update();
}

void AvatarButton::OnProfileWasRemoved(const base::FilePath& profile_path,
                                       const base::string16& profile_name) {
  // If deleting the active profile, don't bother updating the avatar
  // button, as the browser window is being closed anyway.
  if (profile_->GetPath() != profile_path)
    Update();
}

void AvatarButton::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  if (profile_->GetPath() == profile_path)
    Update();
}

void AvatarButton::OnProfileSupervisedUserIdChanged(
    const base::FilePath& profile_path) {
  if (profile_->GetPath() == profile_path)
    Update();
}

void AvatarButton::OnWidgetDestroying(views::Widget* widget) {
  pressed_lock_.reset();
  if (render_native_nav_buttons_)
    SchedulePaint();
  widget_observer_.Remove(widget);
}

void AvatarButton::OnProfileShutdown() {
  // It looks like in some mysterious cases, the AvatarButton outlives the
  // profile (see http://crbug.com/id=579690). The avatar button is owned by
  // the browser frame (which is owned by the BrowserWindow), and there is an
  // expectation for the UI to be destroyed before the profile is destroyed.
  CHECK(false) << "Avatar button must not outlive the profile.";
}

void AvatarButton::Update() {
  // It looks like in some mysterious cases, the AvatarButton outlives the
  // profile manager (see http://crbug.com/id=579690). The avatar button is
  // owned by the browser frame (which is owned by the BrowserWindow), and
  // there is an expectation for the UI to be destroyed before the profile
  // manager is destroyed.
  CHECK(g_browser_process->profile_manager())
      << "Avatar button must not outlive the profile manager";

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  // If we have a single local profile, then use the generic avatar
  // button instead of the profile name. Never use the generic button if
  // the active profile is Guest.
  const bool use_generic_button =
      !profile_->IsGuestSession() && storage.GetNumberOfProfiles() == 1 &&
      !SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated();

  SetText(use_generic_button
              ? base::string16()
              : profiles::GetAvatarButtonTextForProfile(profile_));

  // If the button has no text, clear the text shadows to make sure the
  // image is centered correctly.
  SetTextShadows(
      use_generic_button
          ? gfx::ShadowValues()
          : gfx::ShadowValues(
                10, gfx::ShadowValue(gfx::Vector2d(), 2.0f, SK_ColorDKGRAY)));

  // We want the button to resize if the new text is shorter.
  SetMinSize(gfx::Size());

  if (use_generic_button) {
    SetImage(views::Button::STATE_NORMAL, generic_avatar_);
  } else if (error_controller_.HasAvatarError()) {
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(kSyncProblemIcon, 16, gfx::kGoogleRed700));
  } else {
    SetImage(views::Button::STATE_NORMAL, gfx::ImageSkia());
  }

  // If we are not using the generic button, then reset the spacing between
  // the text and the possible authentication error icon.
  const int kDefaultImageTextSpacing = 5;
  SetImageLabelSpacing(use_generic_button ? 0 : kDefaultImageTextSpacing);

  PreferredSizeChanged();
}

void AvatarButton::SetButtonAvatar(int avatar_idr) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  generic_avatar_ = *rb->GetImageNamed(avatar_idr).ToImageSkia();
}

// TODO(estade): all versions of this button should condense.
bool AvatarButton::IsCondensible() const {
#if defined(OS_WIN)
  return (base::win::GetVersion() >= base::win::VERSION_WIN10) &&
         button_style_ == AvatarButtonStyle::NATIVE;
#else
  return false;
#endif
}

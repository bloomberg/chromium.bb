# Best Practice: Colors

Views best practices for color handling largely center around distinguishing how
colors are handled physically versus logically. Physical colors are defined
presentationally, usually as specific RGB or Material values, or occasionally
referentially (e.g. "#202124", "Google Grey 900", or "10% white atop toolbar
background"). Logical colors are defined functionally, and may be more or less
specific depending on context (e.g. "body text", "call to action", "hovered
button background", or "profile menu bottom stroke").

Distinguishing logical and physical use in code makes code amenable to
systematic changes (Material Design, Harmony, MD Refresh, dark mode...). It
increases behavioral consistency and improves readability. It usually results in
greater UI accessibility. Finally, it helps avoid edge case bugs that are easy
to miss, such as problems with custom themes or the GTK native appearance.

Since colors tend to get scattered across the codebase, mixed with the guidance
on distinguishing physical and logical color use are some rules for consistent
code organization.

[TOC]

## Agree on Color Function

**Get UX/eng agreement on color function, not just presentation.** A mock with
only physical values isn't amenable to the separation described above; there
should be a rationale that describes how the various colors relate to each other
and to the relevant specs (e.g. [Desktop Design System Guidelines](https://goto.google.com/chrome-snowglobe)
(Internal Link)). Ideally, physical color values aren't even necessary, because
all values refer to pre-specified concepts; if new colors are needed, UX
leadership and the toolkit team should agree on how to incorporate them into an
updated system.

## Do Not Use Physical Values Directly In A View

**Do not use physical values directly in a View.** This means avoiding
`SkColorSet[A]RGB(...)`, `SK_ColorBLACK`, `gfx::kGoogleGrey900`, and the like.
Instead, obtain colors by requesting them (by identifier) from an appropriate
theming object. `View`s in `ui/` should call
`GetNativeTheme()->GetSystemColor(id)`; `View`s in `chrome/` should call
`GetThemeProvider()->GetColor(id)`.

|||---|||

#####

**Avoid**

Old code in `tooltip_icon.cc` used hardcoded colors:

#####

**Best Practice**

The [current version](https://source.chromium.org/chromium/chromium/src/+/master:ui/views/bubble/tooltip_icon.cc;l=78;drc=756282c5ac4947c7329bc8b68711c3898334018f)
uses color IDs, allowing the icon colors to potentially be tied to other icon
colors:

|||---|||

|||---|||

#####

``` cpp
void TooltipIcon::SetDrawAsHovered(bool hovered) {
  SetImage(gfx::CreateVectorIcon(
      vector_icons::kInfoOutlineIcon,
      tooltip_icon_size_,

      hovered
          ? SkColorSetARGB(
                0xBD, 0, 0, 0)
          : SkColorSetARGB(
                0xBD, 0x44, 0x44, 0x44)));
}
```

#####

``` cpp
void TooltipIcon::SetDrawAsHovered(bool hovered) {
  SetImage(gfx::CreateVectorIcon(
      vector_icons::kInfoOutlineIcon,
      tooltip_icon_size_,
      GetNativeTheme()->GetSystemColor(
          hovered
              ? ui::NativeTheme::
                    kColorId_TooltipIconHovered
              : ui::NativeTheme::
                    kColorId_TooltipIcon)));
}
```

|||---|||

## Set Colors in OnThemeChanged()

**Set colors in an `OnThemeChanged()` override;** update elsewhere as needed.
Theming objects are generally provided through the `Widget`, and so will be null
in a `View`'s constructor. This isn't a problem because `View`s are not yet
visible at construction. When a `View` is first placed in a `Widget` hierarchy,
and any time afterward that the theme may have changed, it will receive a call
to `OnThemeChanged()`. Thus in the majority of cases, setting colors in
`OnThemeChanged()` handles both initial presentation and theme changes
correctly. In cases where the color to use depends on state, both
`OnThemeChanged()` and the state change handlers may need to call common code
that sets the correct colors.

|||---|||

#####

**Avoid**

Old code in `native_file_system_usage_bubble_view.cc` set colors in its
constructor since it seemed reasonable to do:

#####

**Best Practice**

The [current version](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/ui/views/native_file_system/native_file_system_usage_bubble_view.cc;l=196;drc=532834e1da3874a57dde3ed76511f53b8eb8ecdf)
moves this to `OnThemeChanged()` and thus handles theme changes correctly:

|||---|||


|||---|||

#####

``` cpp
explicit CollapsibleListView(ui::TableModel* model) {
  ...
  auto button =
      views::CreateVectorToggleImageButton(this);








  const SkColor icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->
          GetSystemColor(
              ui::NativeTheme::
                  kColorId_DefaultIconColor);
  views::SetImageFromVectorIconWithColor(
      button.get(), kCaretDownIcon,
      ui::TableModel::kIconSize, icon_color);
  views::SetToggledImageFromVectorIconWithColor(
      button.get(), kCaretUpIcon,
      ui::TableModel::kIconSize, icon_color);
  ...
}
```

#####

``` cpp
explicit CollapsibleListView(ui::TableModel* model) {
  ...
  auto button =
      views::CreateVectorToggleImageButton(this);
  ...
  expand_collapse_button_ =
      label_container->AddChildView(std::move(button));
  ...
}

void CollapsibleListView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const SkColor icon_color = GetNativeTheme()->
      GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);


  views::SetImageFromVectorIconWithColor(
      expand_collapse_button_, kCaretDownIcon,
      ui::TableModel::kIconSize, icon_color);
  views::SetToggledImageFromVectorIconWithColor(
      expand_collapse_button_, kCaretUpIcon,
      ui::TableModel::kIconSize, icon_color);
}

```

|||---|||

## Only Use Colors Locally

**Keep color use local.** `View`s should generally access color IDs only for
themselves, and not get or set colors on other `View`s (e.g. children). Because
the order of `OnThemeChanged()` calls is not guaranteed, getting a cached color
from another `View` may return an incorrect value; and setting physical
properties of other `View`s reduces encapsulation. Instead, convey logical state
changes as necessary, let each `View` manage its own colors, and use file-scope
`View` subclasses freely to define the behavior of specific `View`s.

|||---|||

#####

**Avoid**

Old code in `custom_tab_bar_view.cc` tried to keep a parent's color in sync with
its child:

#####

**Best Practice**

[Current version](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/ui/views/location_bar/custom_tab_bar_view.cc;l=157;drc=ab2ca22fb75c61f7167e9b20ac88cc7a85c3be6b)
queries a color API that is guaranteed to have an up-to-date value:

|||---|||

|||---|||

#####

``` cpp
class CustomTabBarTitleOriginView : public views::View {
 public:
  ...
  SkColor GetLocationColor() const {
    return location_label_->GetEnabledColor();
  }
  ...
 private:
  views::Label* location_label_;
};
```

#####

``` cpp
class CustomTabBarTitleOriginView : public views::View {
 public:
  ...
  SkColor GetLocationColor() const {
    return views::style::GetColor(
        *this, CONTEXT_BODY_TEXT_SMALL,
        views::style::TextStyle::STYLE_PRIMARY);
  }
  ...
};
```

|||---|||

## Only Use Color IDs to Forward Color Information

Where color information must be passed on, **use color IDs, not `SkColor` or
`ImageSkia`**. For example, menu items with images should use
`ImageModel::FromVectorIcon(`...`, id)`, as the other `ImageModel` factory
functions require a physical color at some prior stage, and make handling theme
changes difficult.

|||---|||

#####

Old code in `recovery_install_global_error.cc` embedded colors into the icon
stored in the menu model:

#####

[Current version](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/recovery/recovery_install_global_error.cc;l=70;drc=b25251f1d6bc5359794d98cda8068a94e76ad317)
uses an `ImageModel` to convey just the logical color, allowing consumers to
apply physical colors:

|||---|||

|||---|||

#####

**Avoid**

``` cpp
gfx::Image
RecoveryInstallGlobalError::MenuItemIcon() {
  return gfx::Image(gfx::CreateVectorIcon(
      kBrowserToolsUpdateIcon,
      ui::NativeTheme::GetInstanceForNativeUi()->
          GetSystemColor(
              ui::NativeTheme::
                  kColorId_AlertSeverityHigh)));
}
```

#####

**Best Practice**

``` cpp
ui::ImageModel
RecoveryInstallGlobalError::MenuItemIcon() {
  return ui::ImageModel::FromVectorIcon(
      kBrowserToolsUpdateIcon,
      ui::NativeTheme::
          kColorId_AlertSeverityHigh);
}


```

|||---|||

## Create New Color Identifiers for New UI

For new UI, **add new, specific color identifiers**. If a mock for a new
`FrobberMenu` UI calls for the background color to be the same as the
`FooBarMenu` background, don't reuse `kFooBarMenuBackgroundColor` when
implementing the new menu, as the name will no longer be accurate. Add a
`kFrobberMenuBackgroundColor` identifier and use that instead. `ui/` code should
add identifiers to the [`NATIVE_THEME_COLOR_IDS`](https://source.chromium.org/chromium/chromium/src/+/master:ui/native_theme/native_theme_color_id.h;l=10;drc=21c19ce054e99a4361dff61877a4197831b80e6b)
macro; `chrome/` code should typically add to the
[`NotOverwritableByUserThemeProperty`](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/themes/theme_properties.h;l=91;drc=cfa76e5827628eb2104df0e0b55d5d89f4a93eaf)
enum.

|||---|||

#####

**Avoid**

Old code in `menu_controller_scroll_view_container.cc` reused a text color
constant to paint the drop indicator, since they have the same physical color:

#####

**Best Practice**

[Current version](https://source.chromium.org/chromium/chromium/src/+/master:ui/views/controls/menu/submenu_view.cc;l=229;drc=7910ceae672184033abc44a287e309f14e664b5e)
defines these as distinct logical colors with the same default physical color,
better accommodating platforms where menu text and menu icons may differ:

|||---|||

|||---|||

#####

``` cpp














void SubmenuView::PaintChildren(
    const PaintInfo& paint_info) {
  ...
  const SkColor drop_indicator_color =
      GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::
          kColorId_HighlightedMenuItemForegroundColor);
  recorder.canvas()->FillRect(
      bounds, drop_indicator_color);
}
```

#####

``` cpp
SkColor GetAuraColor(
    NativeTheme::ColorId color_id,
    const NativeTheme* base_theme,
    NativeTheme::ColorScheme color_scheme) {
  switch (color_id) {
    ...
    case NativeTheme::
        kColorId_HighlightedMenuItemForegroundColor:
    case NativeTheme::kColorId_MenuDropIndicator:
      return gfx::kGoogleGrey200;
    ...
  }
}

void SubmenuView::PaintChildren(
    const PaintInfo& paint_info) {
  ...
  const SkColor drop_indicator_color =
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::
              kColorId_MenuDropIndicator);
  recorder.canvas()->FillRect(
      bounds, drop_indicator_color);
}
```

|||---|||

## Define Identifiers Using Relationships

**Define identifiers' physical values by their relationships to other colors**.
In most cases, new UI elements are using the same underlying concepts as other
UI elements. So instead of `kFrobberMenuBackgroundColor` copying the definition
of `kFooBarMenuBackgroundColor`, both menus' colors should probably be defined
in terms of some more generic underlying concept (e.g. a common toolkit-level
menu or background color). Even in more one-off cases, colors can still be
relational; black text on a white background is not simply black in the
abstract, but black because it contrasts with the background. UI colors can
usually similarly be defined as contrasting or blending against other colors,
using the functions in [`color_utils.h`](https://source.chromium.org/chromium/chromium/src/+/master:ui/gfx/color_utils.h;drc=2e98db0e52f3f55056783031ab9f7377d4a12ad5).
In most cases, a correct relational definition for a color will work well in
both light and dark mode, and custom themes as well.

|||---|||

#####

**Avoid**

Old code in `common_theme.cc` defines a disabled button color absolutely:

#####

**Best Practice**

[Current version](https://source.chromium.org/chromium/chromium/src/+/master:ui/native_theme/common_theme.cc;l=264;drc=21c19ce054e99a4361dff61877a4197831b80e6b)
makes it clear that the disabled color is related to the normal color:

|||---|||

|||---|||

#####

``` cpp
SkColor GetAuraColor(
    NativeTheme::ColorId color_id,
    const NativeTheme* base_theme,
    NativeTheme::ColorScheme color_scheme) {
  ...
  switch (color_id) {
    case NativeTheme::
        kColorId_ProminentButtonDisabledColor:
      return gfx::kGoogleGrey100;
    ...





  }
}
```

#####

``` cpp
SkColor GetAuraColor(
    NativeTheme::ColorId color_id,
    const NativeTheme* base_theme,
    NativeTheme::ColorScheme color_scheme) {
  ...
  switch (color_id) {
    case NativeTheme::
        kColorId_ProminentButtonDisabledColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_ButtonColor,
          color_scheme);
      return color_utils::BlendForMinContrast(
          bg, bg, base::nullopt, 1.2f).color;
    }
    ...
  }
}
```

|||---|||

## Keep Image Resources Theme Neutral

**Use image resources that are theme-neutral**. Most vector images should not
encode color information at all; in some cases, [badging](https://source.chromium.org/chromium/chromium/src/+/master:ui/gfx/paint_vector_icon.h;l=70;drc=7a9b1b437ae847e90ef3ac10f73991796dfe5591)
can be used to break a vector into a common core and a set of recolorable
overlays. Full-color raster images that work on any background color can be
used, but images that assume a particular background color, or that are broken
into "light" and "dark" variants, should not be used; these are problematic both
today (with custom themes and GTK) and in the future (if we modify the default
light/dark theme colors or Material Design palettes). These cases are tricky to
address and generally require cooperation between the visual designer, the
toolkit team, and the UI engineer.

[Current code](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.cc;l=100;drc=53d4f4170d068ef7e9b6e19995b0eac0cb3cfd43)
uses two full color raster images, one that assumes a light background and the
other that assumes a dark background:

|||---|||

#####

![Light](colors-upload_violation.png)


#####

![Dark](colors-upload_violation_dark.png)

|||---|||

An improved version could split the image into a fixed color portion and an
uncolored portion using alpha that is programmatically computed based on the
desired foreground color.

## Do Not Use Colors Outside a View

**Do not use colors outside a `View`**, since doing so correctly is difficult.
Global access to theming objects is deprecated and will eventually be removed,
since it's error-prone and hinders future design goals. Direct use of physical
colors and obtaining colors from `View` instances are problematic for reasons
given above. If you need something like this, talk to the toolkit team about the
best approach.

|||---|||

#####

**Avoid**

Old code in `tab_strip_ui_handler.cc` uses global theming objects:

#####

**Best Practice**

[Current version](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/ui/webui/tab_strip/tab_strip_ui_handler.cc;l=524;drc=cfa76e5827628eb2104df0e0b55d5d89f4a93eaf)
gets colors from its embedding `View`; this will still require the caller to
guarantee the `View`'s colors are up to date, monitor for potential changes in
those colors, etc.:

|||---|||

|||---|||

#####

```









void TabStripUIHandler::HandleGetThemeColors(
    const base::ListValue* args) {
  ...
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(
          browser_->profile());
  base::DictionaryValue colors;
  colors.SetString(
      "--tabstrip-background-color",
      color_utils::SkColorToRgbaString(
          tp.GetColor(
              ThemeProperties::COLOR_FRAME)));
  ...
}
```

#####

```
class TabStripUIHandler
    : public content::WebUIMessageHandler,
      public TabStripModelObserver {
  ...
 private:
  TabStripUIEmbedder* const embedder_;
  ...
}

void TabStripUIHandler::HandleGetThemeColors(
    const base::ListValue* args) {
  ...



  base::DictionaryValue colors;
  colors.SetString(
      "--tabstrip-background-color",
      color_utils::SkColorToRgbaString(
          embedder_->GetColor(
              ThemeProperties::COLOR_FRAME)));
  ...
}
```

|||---|||

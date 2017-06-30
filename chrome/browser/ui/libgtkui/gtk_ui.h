// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_GTK_UI_H_
#define CHROME_BROWSER_UI_LIBGTKUI_GTK_UI_H_

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/libgtkui/gtk_signal.h"
#include "chrome/browser/ui/libgtkui/libgtkui_export.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/window/frame_buttons.h"

typedef struct _GParamSpec GParamSpec;
typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;

namespace libgtkui {
class Gtk2KeyBindingsHandler;
class GConfListener;
class DeviceScaleFactorObserver;

// Interface to GTK2 desktop features.
//
class GtkUi : public views::LinuxUI {
 public:
  GtkUi();
  ~GtkUi() override;

  typedef base::Callback<ui::NativeTheme*(aura::Window* window)>
      NativeThemeGetter;

  // Setters used by GConfListener:
  void SetWindowButtonOrdering(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons);
  void SetNonClientMiddleClickAction(NonClientMiddleClickAction action);

  // Called when gtk style changes
  void ResetStyle();

  // ui::LinuxInputMethodContextFactory:
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const override;

  // gfx::LinuxFontDelegate:
  gfx::FontRenderParams GetDefaultFontRenderParams() const override;
  void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      gfx::Font::Weight* weight_out,
      gfx::FontRenderParams* params_out) const override;

  // ui::ShellDialogLinux:
  ui::SelectFileDialog* CreateSelectFileDialog(
      ui::SelectFileDialog::Listener* listener,
      ui::SelectFilePolicy* policy) const override;

  // ui::LinuxUI:
  void Initialize() override;
  bool GetTint(int id, color_utils::HSL* tint) const override;
  bool GetColor(int id, SkColor* color) const override;
  SkColor GetFocusRingColor() const override;
  SkColor GetThumbActiveColor() const override;
  SkColor GetThumbInactiveColor() const override;
  SkColor GetTrackColor() const override;
  SkColor GetActiveSelectionBgColor() const override;
  SkColor GetActiveSelectionFgColor() const override;
  SkColor GetInactiveSelectionBgColor() const override;
  SkColor GetInactiveSelectionFgColor() const override;
  double GetCursorBlinkInterval() const override;
  ui::NativeTheme* GetNativeTheme(aura::Window* window) const override;
  void SetNativeThemeOverride(const NativeThemeGetter& callback) override;
  bool GetDefaultUsesSystemTheme() const override;
  void SetDownloadCount(int count) const override;
  void SetProgressFraction(float percentage) const override;
  bool IsStatusIconSupported() const override;
  std::unique_ptr<views::StatusIconLinux> CreateLinuxStatusIcon(
      const gfx::ImageSkia& image,
      const base::string16& tool_tip) const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size) const override;
  std::unique_ptr<views::Border> CreateNativeBorder(
      views::LabelButton* owning_button,
      std::unique_ptr<views::LabelButtonBorder> border) override;
  void AddWindowButtonOrderObserver(
      views::WindowButtonOrderObserver* observer) override;
  void RemoveWindowButtonOrderObserver(
      views::WindowButtonOrderObserver* observer) override;
  bool UnityIsRunning() override;
  NonClientMiddleClickAction GetNonClientMiddleClickAction() override;
  void NotifyWindowManagerStartupComplete() override;
  void AddDeviceScaleFactorObserver(
      views::DeviceScaleFactorObserver* observer) override;
  void RemoveDeviceScaleFactorObserver(
      views::DeviceScaleFactorObserver* observer) override;

  // ui::TextEditKeybindingDelegate:
  bool MatchEvent(const ui::Event& event,
                  std::vector<ui::TextEditCommandAuraLinux>* commands) override;

  // ui::Views::LinuxUI:
  void UpdateDeviceScaleFactor() override;
  float GetDeviceScaleFactor() const override;

 private:
  typedef std::map<int, SkColor> ColorMap;
  typedef std::map<int, color_utils::HSL> TintMap;

  CHROMEG_CALLBACK_1(GtkUi,
                     void,
                     OnDeviceScaleFactorMaybeChanged,
                     void*,
                     GParamSpec*);

  // This method returns the colors webkit will use for the scrollbars. When no
  // colors are specified by the GTK+ theme, this function averages of the
  // thumb part and of the track colors.
  void SetScrollbarColors();

  // Extracts colors and tints from the GTK theme, both for the
  // ThemeService interface and the colors we send to webkit.
  void LoadGtkValues();

  // Sets the Xcursor theme and size with the GTK theme and size.
  void UpdateCursorTheme();

  // Updates |default_font_*|.
  void UpdateDefaultFont();

  // Gets a ChromeGtkFrame theme color; returns true on success.  No-op on gtk3.
  bool GetChromeStyleColor(const char* sytle_property,
                           SkColor* ret_color) const;

  float GetRawDeviceScaleFactor();

  ui::NativeTheme* native_theme_;

  // On Gtk2, A GtkWindow object with the class "ChromeGtkFrame".  On
  // Gtk3, a regular GtkWindow.
  GtkWidget* fake_window_;

  // Colors calculated by LoadGtkValues() that are given to the
  // caller while |use_gtk_| is true.
  ColorMap colors_;

  // Colors that we pass to WebKit. These are generated each time the theme
  // changes.
  SkColor focus_ring_color_;
  SkColor thumb_active_color_;
  SkColor thumb_inactive_color_;
  SkColor track_color_;
  SkColor active_selection_bg_color_;
  SkColor active_selection_fg_color_;
  SkColor inactive_selection_bg_color_;
  SkColor inactive_selection_fg_color_;

  // Details about the default UI font.
  std::string default_font_family_;
  int default_font_size_pixels_ = 0;
  // Bitfield of gfx::Font::Style values.
  int default_font_style_ = gfx::Font::NORMAL;
  gfx::Font::Weight default_font_weight_ = gfx::Font::Weight::NORMAL;
  gfx::FontRenderParams default_font_render_params_;

#if defined(USE_GCONF)
  // Currently, the only source of window button configuration. This will
  // change if we ever have to support XFCE's configuration system or KDE's.
  std::unique_ptr<GConfListener> gconf_listener_;
#endif  // defined(USE_GCONF)

  // If either of these vectors are non-empty, they represent the current
  // window button configuration.
  std::vector<views::FrameButton> leading_buttons_;
  std::vector<views::FrameButton> trailing_buttons_;

  std::unique_ptr<Gtk2KeyBindingsHandler> key_bindings_handler_;

  // Objects to notify when the window frame button order changes.
  base::ObserverList<views::WindowButtonOrderObserver>
      window_button_order_observer_list_;

  // Objects to notify when the device scale factor changes.
  base::ObserverList<views::DeviceScaleFactorObserver>
      device_scale_factor_observer_list_;

  // Whether we should lower the window on a middle click to the non client
  // area.
  NonClientMiddleClickAction middle_click_action_;

  // Used to override the native theme for a window. If no override is provided
  // or the callback returns nullptr, GtkUi will default to a NativeThemeGtk2
  // instance.
  NativeThemeGetter native_theme_overrider_;

  float device_scale_factor_ = 1.0f;

  DISALLOW_COPY_AND_ASSIGN(GtkUi);
};

}  // namespace libgtkui

// Access point to the GTK2 desktop system. This should be the only symbol that
// is exported in the library; everything else should be used through the
// interface, because eventually this .so will be loaded through dlopen at
// runtime so our main binary can conditionally load GTK2 or GTK3 or EFL or
// QT or whatever.
LIBGTKUI_EXPORT views::LinuxUI* BuildGtkUi();

#endif  // CHROME_BROWSER_UI_LIBGTKUI_GTK_UI_H_

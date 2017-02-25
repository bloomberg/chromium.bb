// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WALLPAPER_WALLPAPER_CONTROLLER_H_
#define ASH_COMMON_WALLPAPER_WALLPAPER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm_display_observer.h"
#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/wallpaper/wallpaper_color_calculator_observer.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "components/wallpaper/wallpaper_resizer_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class TaskRunner;
}

namespace wallpaper {
class WallpaperColorCalculator;
class WallpaperResizer;
}

namespace ash {

class WallpaperControllerObserver;

// Controls the desktop background wallpaper.
class ASH_EXPORT WallpaperController
    : public NON_EXPORTED_BASE(mojom::WallpaperController),
      public WmDisplayObserver,
      public ShellObserver,
      public wallpaper::WallpaperResizerObserver,
      public wallpaper::WallpaperColorCalculatorObserver {
 public:
  enum WallpaperMode { WALLPAPER_NONE, WALLPAPER_IMAGE };

  explicit WallpaperController(
      const scoped_refptr<base::TaskRunner>& task_runner);
  ~WallpaperController() override;

  // Binds the mojom::WallpaperController interface request to this object.
  void BindRequest(mojom::WallpaperControllerRequest request);

  // Add/Remove observers.
  void AddObserver(WallpaperControllerObserver* observer);
  void RemoveObserver(WallpaperControllerObserver* observer);

  SkColor prominent_color() const { return prominent_color_; }

  // Provides current image on the wallpaper, or empty gfx::ImageSkia if there
  // is no image, e.g. wallpaper is none.
  gfx::ImageSkia GetWallpaper() const;

  wallpaper::WallpaperLayout GetWallpaperLayout() const;

  // Sets the wallpaper and alerts observers of changes.
  void SetWallpaperImage(const gfx::ImageSkia& image,
                         wallpaper::WallpaperLayout layout);

  // Creates an empty wallpaper. Some tests require a wallpaper widget is ready
  // when running. However, the wallpaper widgets are now created
  // asynchronously. If loading a real wallpaper, there are cases that these
  // tests crash because the required widget is not ready. This function
  // synchronously creates an empty widget for those tests to prevent
  // crashes. An example test is SystemGestureEventFilterTest.ThreeFingerSwipe.
  void CreateEmptyWallpaper();

  // Move all wallpaper widgets to the locked container.
  // Returns true if the wallpaper moved.
  bool MoveToLockedContainer();

  // Move all wallpaper widgets to unlocked container.
  // Returns true if the wallpaper moved.
  bool MoveToUnlockedContainer();

  // WmDisplayObserver:
  void OnDisplayConfigurationChanged() override;

  // ShellObserver:
  void OnRootWindowAdded(WmWindow* root_window) override;

  // Returns the maximum size of all displays combined in native
  // resolutions.  Note that this isn't the bounds of the display who
  // has maximum resolutions. Instead, this returns the size of the
  // maximum width of all displays, and the maximum height of all displays.
  static gfx::Size GetMaxDisplaySizeInNative();

  // Returns true if the specified wallpaper is already stored
  // in |current_wallpaper_|.
  // If |compare_layouts| is false, layout is ignored.
  bool WallpaperIsAlreadyLoaded(const gfx::ImageSkia& image,
                                bool compare_layouts,
                                wallpaper::WallpaperLayout layout) const;

  void set_wallpaper_reload_delay_for_test(int value) {
    wallpaper_reload_delay_ = value;
  }

  // Opens the set wallpaper page in the browser.
  void OpenSetWallpaperPage();

  // mojom::WallpaperController overrides:
  void SetWallpaperPicker(mojom::WallpaperPickerPtr picker) override;
  void SetWallpaper(const SkBitmap& wallpaper,
                    wallpaper::WallpaperLayout layout) override;

  // WallpaperResizerObserver:
  void OnWallpaperResized() override;

  // WallpaperColorCalculatorObserver:
  void OnColorCalculationComplete() override;

 private:
  // Returns true if the shelf should be colored based on the wallpaper's
  // prominent color. |luma| and |saturation| are output parameters.
  static bool GetProminentColorProfile(
      color_utils::LumaRange* luma,
      color_utils::SaturationRange* saturation);

  // Creates a WallpaperWidgetController for |root_window|.
  void InstallDesktopController(WmWindow* root_window);

  // Creates a WallpaperWidgetController for all root windows.
  void InstallDesktopControllerForAllWindows();

  // Moves the wallpaper to the specified container across all root windows.
  // Returns true if a wallpaper moved.
  bool ReparentWallpaper(int container);

  // Returns the wallpaper container id for unlocked and locked states.
  int GetWallpaperContainerId(bool locked);

  // Reload the wallpaper. |clear_cache| specifies whether to clear the
  // wallpaper cahce or not.
  void UpdateWallpaper(bool clear_cache);

  // Sets |prominent_color_| and notifies the observers if there is a change.
  void SetProminentColor(SkColor color);

  // If enabled, will initiates an asynchronous task to extract colors from the
  // wallpaper. If an existing calculation is in progress it is destroyed.
  void CalculateWallpaperColors();

  bool locked_;

  WallpaperMode wallpaper_mode_;

  // Wallpaper picker interface in chrome browser, used to open the picker.
  mojom::WallpaperPickerPtr wallpaper_picker_;

  // Bindings for the WallpaperController interface.
  mojo::BindingSet<mojom::WallpaperController> bindings_;

  base::ObserverList<WallpaperControllerObserver> observers_;

  std::unique_ptr<wallpaper::WallpaperResizer> current_wallpaper_;

  // Asynchronous task to extract colors from the wallpaper.
  std::unique_ptr<wallpaper::WallpaperColorCalculator> color_calculator_;

  // The prominent color extracted from the current wallpaper.
  // SK_ColorTRANSPARENT is used by default or if extracting colors fails.
  SkColor prominent_color_;

  gfx::Size current_max_display_size_;

  base::OneShotTimer timer_;

  int wallpaper_reload_delay_;

  scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperController);
};

}  // namespace ash

#endif  // ASH_COMMON_WALLPAPER_WALLPAPER_CONTROLLER_H_

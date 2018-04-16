// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FIRST_RUN_FIRST_RUN_HELPER_H_
#define ASH_FIRST_RUN_FIRST_RUN_HELPER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/first_run_helper.mojom.h"
#include "ash/wm/overlay_event_filter.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace views {
class Widget;
}

namespace ash {

class DesktopCleaner;

// Interface used by first-run tutorial to manipulate and retrieve information
// about shell elements.
class ASH_EXPORT FirstRunHelper : public mojom::FirstRunHelper,
                                  public OverlayEventFilter::Delegate {
 public:
  class Observer {
   public:
    // Called when first-run UI was cancelled.
    virtual void OnCancelled() = 0;
    virtual ~Observer() {}
  };

 public:
  FirstRunHelper();
  ~FirstRunHelper() override;

  void BindRequest(mojom::FirstRunHelperRequest request);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns widget to place tutorial UI into it.
  // TODO(jamescook): Migrate widget into chrome and remove these methods.
  views::Widget* GetOverlayWidget();
  void CreateOverlayWidget();
  void CloseOverlayWidget();

  // mojom::FirstRunHelper:
  void GetAppListButtonBounds(GetAppListButtonBoundsCallback cb) override;
  void OpenTrayBubble(OpenTrayBubbleCallback cb) override;
  void CloseTrayBubble() override;
  void GetHelpButtonBounds(GetHelpButtonBoundsCallback cb) override;

  // OverlayEventFilter::Delegate:
  void Cancel() override;
  bool IsCancelingKeyEvent(ui::KeyEvent* event) override;
  aura::Window* GetWindow() override;

 private:
  // Bindings for clients of the mojo interface.
  mojo::BindingSet<mojom::FirstRunHelper> bindings_;

  // TODO(jamescook): Convert to mojo observer.
  base::ObserverList<Observer> observers_;

  // The first run dialog window.
  views::Widget* widget_ = nullptr;

  std::unique_ptr<DesktopCleaner> cleaner_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelper);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_H_

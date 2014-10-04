// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PANELS_PANEL_STACK_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PANELS_PANEL_STACK_WINDOW_COCOA_H_

#import <Cocoa/Cocoa.h>

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel_stack_window.h"
#include "ui/gfx/rect.h"

@class BatchBoundsAnimationDelegate;
class Panel;
class PanelStackWindowCocoa;

// A native window that acts as the owner of all/partial panels in the stack,
// in order to make these panels appear as a single window.
class PanelStackWindowCocoa : public NativePanelStackWindow {
 public:
  explicit PanelStackWindowCocoa(NativePanelStackWindowDelegate* delegate);
  virtual ~PanelStackWindowCocoa();

  // Notified by BatchBoundsAnimationDelegate about the ending of the bounds
  // animation.
  void BoundsUpdateAnimationEnded();

 protected:
  // Overridden from NativePanelStackWindow:
  virtual void Close() override;
  virtual void AddPanel(Panel* panel) override;
  virtual void RemovePanel(Panel* panel) override;
  virtual void MergeWith(NativePanelStackWindow* another) override;
  virtual bool IsEmpty() const override;
  virtual bool HasPanel(Panel* panel) const override;
  virtual void MovePanelsBy(const gfx::Vector2d& delta) override;
  virtual void BeginBatchUpdatePanelBounds(bool animate) override;
  virtual void AddPanelBoundsForBatchUpdate(Panel* panel,
                                            const gfx::Rect& bounds) override;
  virtual void EndBatchUpdatePanelBounds() override;
  virtual bool IsAnimatingPanelBounds() const override;
  virtual void Minimize() override;
  virtual bool IsMinimized() const override;
  virtual void DrawSystemAttention(bool draw_attention) override;
  virtual void OnPanelActivated(Panel* panel) override;

 private:
  typedef std::list<Panel*> Panels;

  // The map value is old bounds of the panel.
  typedef std::map<Panel*, gfx::Rect> BoundsUpdates;

  // Terminates current bounds animation, if any.
  void TerminateBoundsAnimation();

  // Computes/updates the minimum bounds that could fit all panels.
  gfx::Rect GetStackWindowBounds() const;
  void UpdateStackWindowBounds();

  // Ensures that the background window is created.
  void EnsureWindowCreated();

  // Weak pointer. Owned by PanelManager.
  NativePanelStackWindowDelegate* delegate_;

  // The underlying window.
  base::scoped_nsobject<NSWindow> window_;

  Panels panels_;

  // Identifier from requestUserAttention.
  NSInteger attention_request_id_;

  // For batch bounds update.
  bool bounds_updates_started_;
  bool animate_bounds_updates_;
  BoundsUpdates bounds_updates_;

  // Used to animate the bounds changes at a synchronized pace.
  // Lifetime controlled manually, needs more than just |release| to terminate.
  NSViewAnimation* bounds_animation_;
  base::scoped_nsobject<BatchBoundsAnimationDelegate>
      bounds_animation_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PanelStackWindowCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_PANELS_PANEL_STACK_WINDOW_COCOA_H_

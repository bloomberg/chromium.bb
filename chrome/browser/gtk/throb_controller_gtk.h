// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_THROB_CONTROLLER_GTK_H_
#define CHROME_BROWSER_GTK_THROB_CONTROLLER_GTK_H_

#include "app/throb_animation.h"
#include "base/scoped_ptr.h"

typedef struct _GtkWidget GtkWidget;

// This class handles the "throbbing" of a GtkChromeButton. The visual effect
// of throbbing is created by painting partially transparent hover effects. It
// only works in non-gtk theme mode. This class mainly exists to glue an
// AnimationDelegate (C++ class) to a GtkChromeButton* (GTK/c object). Usage is
// as follows:
//
//   GtkWidget* button = gtk_chrome_button_new();
//   ThrobControllerGtk::ThrobFor(button);
//
// The ThrobFor() function will handle creation of the ThrobControllerGtk
// object, which will delete itself automatically when the throbbing is done,
// or when the widget is destroyed. It may also be canceled prematurely with
//
//   ThrobControllerGtk* throbber =
//       ThrobControllerGtk::GetThrobControllerGtk(button);
//   throbber->Destroy();
class ThrobControllerGtk : public AnimationDelegate {
 public:
  virtual ~ThrobControllerGtk();

  GtkWidget* button() { return button_; }

  // Throb for |cycles| cycles. This will override the current remaining
  // number of cycles.
  void StartThrobbing(int cycles);

  // Get the ThrobControllerGtk for a given GtkChromeButton*. It is an error
  // to call this on a widget that is not a GtkChromeButton*.
  static ThrobControllerGtk* GetThrobControllerGtk(GtkWidget* button);

  // Make |button| throb. It is an error to try to have two ThrobControllerGtk
  // instances for one GtkChromeButton*.
  static void ThrobFor(GtkWidget* button);

  // Stop throbbing and delete |this|.
  void Destroy();

 private:
  explicit ThrobControllerGtk(GtkWidget* button);

  // Overridden from AnimationDelegate.
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);

  static void OnButtonDestroy(GtkWidget* widget,
                              ThrobControllerGtk* button);

  ThrobAnimation animation_;
  GtkWidget* button_;

  DISALLOW_COPY_AND_ASSIGN(ThrobControllerGtk);
};

#endif  // CHROME_BROWSER_GTK_THROB_CONTROLLER_GTK_H_

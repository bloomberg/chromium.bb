// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}

class PasswordAccessoryViewInterface;

// Concrete, implementation of the controller for the view located beneath the
// keyboard accessory.
// When a new instance is requested with |GetOrCreate|, the returned weak_ptr
// will point to a controller which communicates with a UI element that is
// created if it doesn't exist yet.
// As soon as the UI element is destroyed, it destroys all its counterparts on
// the native side as well and the obtained weak_ptr will be invalidated.
class PasswordAccessoryController {
 public:
  ~PasswordAccessoryController();

  // If the passed in |previous| instance for this page is still valid, it will
  // be returned.
  // Otherwise, this creates a new controller owned by a view it creates.
  // The passed in |container_view| is the current web page view.
  // It hosts the accessory UI which is needed to display the credentials.
  static base::WeakPtr<PasswordAccessoryController> GetOrCreate(
      base::WeakPtr<PasswordAccessoryController> previous,
      gfx::NativeView container_view);

  // Notifies the view about credentials to be displayed.
  void OnPasswordsAvailable(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const GURL& origin);

  // Destroys the view. As the controller is owned by it, this will destroy the
  // controller as well.
  void Destroy();

  // The web page view containing the focused field.
  gfx::NativeView container_view();

  // Creates a controller instance with the given |view|.
  // Different from |GetOrCreate|, this function doesn't bind the
  // controller to the |view| but returns the new instance.
  static std::unique_ptr<PasswordAccessoryController> CreateForTesting(
      std::unique_ptr<PasswordAccessoryViewInterface> view);

#if defined(UNIT_TEST)
  // This creates a new weak_ptr. Outside of tests, use |GetOrCreate|.
  base::WeakPtr<PasswordAccessoryController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Returns the held view for testing.
  PasswordAccessoryViewInterface* view() { return view_.get(); }
#endif  // defined(UNIT_TEST)

 private:
  explicit PasswordAccessoryController(gfx::NativeView container_view);

  // Hold the native instance of the bridge until the bridge deletes it.
  std::unique_ptr<PasswordAccessoryViewInterface> view_;

  // The page this accessory sheet and the focused field lives in.
  const gfx::NativeView container_view_;

  base::WeakPtrFactory<PasswordAccessoryController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryController);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_

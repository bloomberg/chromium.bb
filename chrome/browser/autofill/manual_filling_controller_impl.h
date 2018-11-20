// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "content/public/browser/web_contents_user_data.h"

class PasswordAccessoryController;

// Use ManualFillingController::GetOrCreate to obtain instances of this class.
class ManualFillingControllerImpl
    : public ManualFillingController,
      public content::WebContentsUserData<ManualFillingControllerImpl> {
 public:
  ~ManualFillingControllerImpl() override;

  // ManualFillingController:
  void RefreshSuggestionsForField(
      bool is_fillable,
      const autofill::AccessorySheetData& accessory_sheet_data) override;
  void OnFilledIntoFocusedField(autofill::FillingStatus status) override;
  void ShowWhenKeyboardIsVisible() override;
  void Hide() override;
  void OnAutomaticGenerationStatusChanged(bool available) override;
  void OnFillingTriggered(bool is_password,
                          const base::string16& text_to_fill) override;
  void OnOptionSelected(const base::string16& selected_option) const override;
  void OnGenerationRequested() override;
  void GetFavicon(
      int desired_size_in_pixel,
      base::OnceCallback<void(const gfx::Image&)> icon_callback) override;
  gfx::NativeView container_view() const override;

  // Returns a weak pointer for this object.
  base::WeakPtr<ManualFillingController> AsWeakPtr();

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows injecting a fake/mock
  // view and type-specific controllers.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<PasswordAccessoryController> pwd_controller,
      std::unique_ptr<ManualFillingViewInterface> test_view);

#if defined(UNIT_TEST)
  // Returns the held view for testing.
  ManualFillingViewInterface* view() const { return view_.get(); }
#endif  // defined(UNIT_TEST)

 private:
  friend class content::WebContentsUserData<ManualFillingControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit ManualFillingControllerImpl(content::WebContents* contents);

  // Constructor that allows to inject a mock or fake view.
  ManualFillingControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<PasswordAccessoryController> pwd_controller,
      std::unique_ptr<ManualFillingViewInterface> view);

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // The password accessory controller object to forward view requests to.
  base::WeakPtr<PasswordAccessoryController> pwd_controller_;

  // Hold the native instance of the view. Must be last declared and initialized
  // member so the view can be created in the constructor with a fully set up
  // controller instance.
  std::unique_ptr<ManualFillingViewInterface> view_;

  base::WeakPtrFactory<ManualFillingControllerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManualFillingControllerImpl);
};

#endif  // CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_

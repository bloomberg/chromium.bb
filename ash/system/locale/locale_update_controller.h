// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_H_
#define ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_H_

#include <string>

#include "ash/public/interfaces/locale.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

class LocaleChangeObserver {
 public:
  virtual ~LocaleChangeObserver() = default;

  // Called when locale is changed.
  virtual void OnLocaleChanged() = 0;
};

// Observes and handles locale change events.
class LocaleUpdateController : public mojom::LocaleUpdateController {
 public:
  LocaleUpdateController();
  ~LocaleUpdateController() override;

  // Binds the mojom::LocaleUpdateController interface request to this
  // object.
  void BindRequest(mojom::LocaleUpdateControllerRequest request);

  void AddObserver(LocaleChangeObserver* observer);
  void RemoveObserver(LocaleChangeObserver* observer);

 private:
  // Overridden from mojom::LocaleUpdateController:
  void OnLocaleChanged(const std::string& cur_locale,
                       const std::string& from_locale,
                       const std::string& to_locale,
                       OnLocaleChangedCallback callback) override;

  std::string cur_locale_;
  std::string from_locale_;
  std::string to_locale_;
  base::ObserverList<LocaleChangeObserver>::Unchecked observers_;

  // Bindings for the LocaleUpdateController interface.
  mojo::BindingSet<mojom::LocaleUpdateController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(LocaleUpdateController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_H_

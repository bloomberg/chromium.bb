// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/common/extensions/api/braille_display_private.h"

namespace extensions {
namespace api {
namespace braille_display_private {

// Stub implementation of BrailleController for use when brlapi is not
// enabled.
class BrailleControllerImpl : public BrailleController {
 public:
  static BrailleControllerImpl* GetInstance();
  virtual scoped_ptr<DisplayState> GetDisplayState() OVERRIDE;
  virtual void WriteDots(const std::string& cells) OVERRIDE;
  virtual void AddObserver(BrailleObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BrailleObserver* observer) OVERRIDE;

 private:
  BrailleControllerImpl();
  virtual ~BrailleControllerImpl();
  friend struct DefaultSingletonTraits<BrailleControllerImpl>;
  DISALLOW_COPY_AND_ASSIGN(BrailleControllerImpl);
};

BrailleController::BrailleController() {
}

BrailleController::~BrailleController() {
}

// static
BrailleController* BrailleController::GetInstance() {
  return BrailleControllerImpl::GetInstance();
}

BrailleControllerImpl::BrailleControllerImpl() {
}

BrailleControllerImpl::~BrailleControllerImpl() {
}

// static
BrailleControllerImpl* BrailleControllerImpl::GetInstance() {
  return Singleton<BrailleControllerImpl,
                   LeakySingletonTraits<BrailleControllerImpl> >::get();
}

scoped_ptr<DisplayState> BrailleControllerImpl::GetDisplayState() {
  return scoped_ptr<DisplayState>(new DisplayState()).Pass();
}

void BrailleControllerImpl::WriteDots(const std::string& cells) {
}

void BrailleControllerImpl::AddObserver(BrailleObserver* observer) {
}

void BrailleControllerImpl::RemoveObserver(BrailleObserver* observer) {
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

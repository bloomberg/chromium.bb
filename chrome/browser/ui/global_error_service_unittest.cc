// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error_service.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/global_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Error base class that keeps track of the number of errors that exist.
class BaseError : public GlobalError {
 public:
  BaseError() { ++count_; }
  virtual ~BaseError() { --count_; }

  static int count() { return count_; }

  bool HasBadge() OVERRIDE { return false; }
  virtual int GetBadgeResourceID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }

  virtual bool HasMenuItem() OVERRIDE { return false; }
  virtual int MenuItemCommandID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }
  virtual string16 MenuItemLabel() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual int MenuItemIconResourceID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE { ADD_FAILURE(); }

  virtual bool HasBubbleView() OVERRIDE { return false; }
  virtual void ShowBubbleView(Browser* browser) OVERRIDE { ADD_FAILURE(); }
  virtual int GetBubbleViewIconResourceID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }
  virtual string16 GetBubbleViewTitle() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual string16 GetBubbleViewMessage() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual void BubbleViewDidClose() OVERRIDE { ADD_FAILURE(); }
  virtual void BubbleViewAcceptButtonPressed() OVERRIDE { ADD_FAILURE(); }
  virtual void BubbleViewCancelButtonPressed() OVERRIDE { ADD_FAILURE(); }

 private:
  // This tracks the number BaseError objects that are currently instantiated.
  static int count_;

  DISALLOW_COPY_AND_ASSIGN(BaseError);
};

int BaseError::count_ = 0;

// A simple error that only has a badge.
class BadgeError : public BaseError {
 public:
  explicit BadgeError(int resource_id) : resource_id_(resource_id) {}
  bool HasBadge() OVERRIDE { return true; }
  virtual int GetBadgeResourceID() OVERRIDE { return resource_id_; }

 private:
  int resource_id_;

  DISALLOW_COPY_AND_ASSIGN(BadgeError);
};

// A simple error that only has a menu item.
class MenuError : public BaseError {
 public:
  explicit MenuError(int command_id) : command_id_(command_id) {}

  virtual bool HasMenuItem() OVERRIDE { return true; }
  virtual int MenuItemCommandID() OVERRIDE { return command_id_; }
  virtual string16 MenuItemLabel() OVERRIDE { return string16(); }
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE {}

 private:
  int command_id_;

  DISALLOW_COPY_AND_ASSIGN(MenuError);
};

} // namespace

// Test adding errors to the global error service.
TEST(GlobalErrorServiceTest, AddError) {
  scoped_ptr<GlobalErrorService> service(new GlobalErrorService(NULL));
  EXPECT_EQ(0u, service->errors().size());

  BaseError* error1 = new BaseError;
  service->AddGlobalError(error1);
  EXPECT_EQ(1u, service->errors().size());
  EXPECT_EQ(error1, service->errors()[0]);

  BaseError* error2 = new BaseError;
  service->AddGlobalError(error2);
  EXPECT_EQ(2u, service->errors().size());
  EXPECT_EQ(error1, service->errors()[0]);
  EXPECT_EQ(error2, service->errors()[1]);

  // Ensure that deleting the service deletes the error objects.
  EXPECT_EQ(2, BaseError::count());
  service.reset();
  EXPECT_EQ(0, BaseError::count());
}

// Test removing errors from the global error service.
TEST(GlobalErrorServiceTest, RemoveError) {
  scoped_ptr<GlobalErrorService> service(new GlobalErrorService(NULL));
  BaseError error1;
  service->AddGlobalError(&error1);
  BaseError error2;
  service->AddGlobalError(&error2);

  EXPECT_EQ(2u, service->errors().size());
  service->RemoveGlobalError(&error1);
  EXPECT_EQ(1u, service->errors().size());
  EXPECT_EQ(&error2, service->errors()[0]);
  service->RemoveGlobalError(&error2);
  EXPECT_EQ(0u, service->errors().size());

  // Ensure that deleting the service does not delete the error objects.
  EXPECT_EQ(2, BaseError::count());
  service.reset();
  EXPECT_EQ(2, BaseError::count());
}

// Test finding errors by their menu item command ID.
TEST(GlobalErrorServiceTest, GetMenuItem) {
  BaseError* error1 = new BaseError;
  MenuError* error2 = new MenuError(2);
  MenuError* error3 = new MenuError(3);

  GlobalErrorService service(NULL);
  service.AddGlobalError(error1);
  service.AddGlobalError(error2);
  service.AddGlobalError(error3);

  EXPECT_EQ(error2, service.GetGlobalErrorByMenuItemCommandID(2));
  EXPECT_EQ(error3, service.GetGlobalErrorByMenuItemCommandID(3));
  EXPECT_EQ(NULL, service.GetGlobalErrorByMenuItemCommandID(4));
}

// Test getting the badge icon resource ID of the first error.
TEST(GlobalErrorServiceTest, GetBadgeID) {
  BaseError* error1 = new BaseError;
  BadgeError error2(2);
  BadgeError* error3 = new BadgeError(3);

  GlobalErrorService service(NULL);
  EXPECT_EQ(0, service.GetFirstBadgeResourceID());

  service.AddGlobalError(error1);
  EXPECT_EQ(0, service.GetFirstBadgeResourceID());

  service.AddGlobalError(&error2);
  EXPECT_EQ(2, service.GetFirstBadgeResourceID());

  service.AddGlobalError(error3);
  EXPECT_EQ(2, service.GetFirstBadgeResourceID());

  // Remove the first error with a badge.
  service.RemoveGlobalError(&error2);
  // Now error3 should be the first error with a badge.
  EXPECT_EQ(3, service.GetFirstBadgeResourceID());
}

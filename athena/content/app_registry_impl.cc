// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/app_registry.h"

#include "athena/content/app_activity_registry.h"
#include "athena/content/public/app_content_control_delegate.h"
#include "base/logging.h"

namespace athena {

class AppRegistryImpl : public AppRegistry {
 public:
  AppRegistryImpl();
  virtual ~AppRegistryImpl();

  // AppRegistry:
  virtual void SetDelegate(AppContentControlDelegate* delegate) OVERRIDE;
  virtual AppContentControlDelegate* GetDelegate() OVERRIDE;
  virtual AppActivityRegistry* GetAppActivityRegistry(
      const std::string& app_id,
      content::BrowserContext* browser_context) OVERRIDE;
  virtual int NumberOfApplications() const OVERRIDE { return app_list_.size(); }

 private:
  virtual void RemoveAppActivityRegistry(
      AppActivityRegistry* registry) OVERRIDE;

 std::vector<AppActivityRegistry*> app_list_;

  scoped_ptr<AppContentControlDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppRegistryImpl);
};

namespace {

AppRegistryImpl* instance = NULL;

}  // namespace

AppRegistryImpl::AppRegistryImpl() :
    delegate_(AppContentControlDelegate::CreateAppContentControlDelegate()) {}

AppRegistryImpl::~AppRegistryImpl() {
  DCHECK(app_list_.empty());
}

void AppRegistryImpl::SetDelegate(AppContentControlDelegate* delegate) {
  DCHECK(delegate);
  delegate_.reset(delegate);
}

AppContentControlDelegate* AppRegistryImpl::GetDelegate() {
  return delegate_.get();
}

AppActivityRegistry* AppRegistryImpl::GetAppActivityRegistry(
    const std::string& app_id,
    content::BrowserContext* browser_context) {
  // Search for an existing proxy.
  for (std::vector<AppActivityRegistry*>::iterator it = app_list_.begin();
       it != app_list_.end(); ++it) {
    if ((*it)->app_id() == app_id &&
        (*it)->browser_context() == browser_context)
      return *it;
  }

  // Create and return a new application object.
  AppActivityRegistry* app_activity_registry =
      new AppActivityRegistry(app_id, browser_context);
  app_list_.push_back(app_activity_registry);
  return app_activity_registry;
}

void AppRegistryImpl::RemoveAppActivityRegistry(AppActivityRegistry* registry) {
  std::vector<AppActivityRegistry*>::iterator item =
      std::find(app_list_.begin(), app_list_.end(), registry);
  CHECK(item != app_list_.end());
  app_list_.erase(item);
  delete registry;
}

// static
void AppRegistry::Create() {
  DCHECK(!instance);
  instance = new AppRegistryImpl();
}

// static
AppRegistry* AppRegistry::Get() {
  DCHECK(instance);
  return instance;
}

// static
void AppRegistry::ShutDown() {
  DCHECK(instance);
  delete instance;
}

AppRegistry::AppRegistry() {}

AppRegistry::~AppRegistry() {
  instance = NULL;
}

}  // namespace athena

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_REGISTRY_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ShellWindow;

// The ShellWindowRegistry tracks the ShellWindows for all platform apps for a
// particular profile.
// This class is planned to evolve into tracking all PlatformApps for a
// particular profile, with a PlatformApp encapsulating all views (background
// page, shell windows, tray view, panels etc.) and other app level behaviour
// (e.g. notifications the app is interested in, lifetime of the background
// page).
class ShellWindowRegistry : public ProfileKeyedService {
 public:
  typedef std::set<ShellWindow*> ShellWindowSet;
  typedef ShellWindowSet::const_iterator const_iterator;

  ShellWindowRegistry();
  virtual ~ShellWindowRegistry();

  // Returns the instance for the given profile, or NULL if none. This is
  // a convenience wrapper around ShellWindowRegistryFactory::GetForProfile.
  static ShellWindowRegistry* Get(Profile* profile);

  void AddShellWindow(ShellWindow* shell_window);
  void RemoveShellWindow(ShellWindow* shell_window);

  const ShellWindowSet& shell_windows() const { return shell_windows_; }

 private:
  class Factory : public ProfileKeyedServiceFactory {
   public:
    static ShellWindowRegistry* GetForProfile(Profile* profile);

    static Factory* GetInstance();
   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // ProfileKeyedServiceFactory
    virtual ProfileKeyedService* BuildServiceInstanceFor(
        Profile* profile) const OVERRIDE;
    virtual bool ServiceIsCreatedWithProfile() OVERRIDE;
    virtual bool ServiceIsNULLWhileTesting() OVERRIDE;
  };

  ShellWindowSet shell_windows_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_REGISTRY_H_

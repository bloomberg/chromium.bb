// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_CONTROLLER_H_

#include <vector>

#include "chrome/browser/extensions/component_migration_helper.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"

using extensions::ComponentMigrationHelper;

// Controller for MediaRouterAction that determines when to show and hide the
// action icon on the toolbar. There should be one instance of this class per
// profile, and it should only be used on the UI thread.
class MediaRouterActionController : public media_router::IssuesObserver,
                                    public media_router::MediaRoutesObserver {
 public:
  explicit MediaRouterActionController(Profile* profile);
  ~MediaRouterActionController() override;

  // media_router::IssuesObserver:
  void OnIssueUpdated(const media_router::Issue* issue) override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const std::vector<media_router::MediaRoute>& routes,
                       const std::vector<media_router::MediaRoute::Id>&
                           joinable_route_ids) override;

 private:
  friend class MediaRouterActionControllerUnitTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterActionControllerUnitTest, EphemeralIcon);

  // Constructor for injecting dependencies in tests.
  MediaRouterActionController(
      Profile* profile,
      media_router::MediaRouter* router,
      ComponentMigrationHelper::ComponentActionDelegate*
          component_action_delegate,
      ComponentMigrationHelper* component_migration_helper);

  // Adds or removes the Media Router action icon to/from
  // |component_action_delegate_| if necessary, depending on whether or not
  // we have issues or local routes.
  void MaybeAddOrRemoveAction();

  // Returns |true| if the Media Router action should be present on the toolbar
  // or the overflow menu.
  bool ShouldEnableAction() const;

  // The profile |this| is associated with. There should be one instance of this
  // class per profile.
  Profile* const profile_;

  // The delegate that is responsible for showing and hiding the icon on the
  // toolbar. It outlives |this|.
  ComponentMigrationHelper::ComponentActionDelegate* const
      component_action_delegate_;

  // Responsible for changing the pref to always show or hide component actions.
  // It is owned by ToolbarActionsModel and outlives |this|.
  ComponentMigrationHelper* const component_migration_helper_;

  bool has_issue_ = false;
  bool has_local_display_route_ = false;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterActionController);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_CONTROLLER_H_

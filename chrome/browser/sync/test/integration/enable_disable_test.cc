// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/unified_consent/feature.h"

using base::FeatureList;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::ModelTypeFromString;
using syncer::ModelTypeToString;
using syncer::ProxyTypes;
using syncer::SyncPrefs;
using syncer::UserSelectableTypes;

namespace {

// Some types show up in multiple groups. This means that there are at least two
// user selectable groups that will cause these types to become enabled. This
// affects our tests because we cannot assume that before enabling a multi type
// it will be disabled, because the other selectable type(s) could already be
// enabling it. And vice versa for disabling.
ModelTypeSet MultiGroupTypes(const SyncPrefs& sync_prefs,
                             const ModelTypeSet& registered_types) {
  const ModelTypeSet selectable_types = UserSelectableTypes();
  ModelTypeSet seen;
  ModelTypeSet multi;
  // TODO(vitaliii): Do not use such short variable names here (and possibly
  // elsewhere in the file).
  for (ModelType st : selectable_types) {
    const ModelTypeSet grouped_types = sync_prefs.ResolvePrefGroups(
        registered_types, ModelTypeSet(st),
        unified_consent::IsUnifiedConsentFeatureEnabled());
    for (ModelType gt : grouped_types) {
      if (seen.Has(gt)) {
        multi.Put(gt);
      } else {
        seen.Put(gt);
      }
    }
  }
  return multi;
}

// This test enables and disables types and verifies the type is sufficiently
// affected by checking for existence of a root node.
class EnableDisableSingleClientTest : public SyncTest {
 public:
  EnableDisableSingleClientTest() : SyncTest(SINGLE_CLIENT) {}
  ~EnableDisableSingleClientTest() override {}

  // Don't use self-notifications as they can trigger additional sync cycles.
  bool TestUsesSelfNotifications() override { return false; }

  bool ModelTypeExists(ModelType type) {
    base::RunLoop loop;
    std::unique_ptr<base::ListValue> all_nodes;
    GetSyncService(0)->GetAllNodes(
        base::BindLambdaForTesting([&](std::unique_ptr<base::ListValue> nodes) {
          all_nodes = std::move(nodes);
          loop.Quit();
        }));
    loop.Run();
    // Look for the root node corresponding to |type|.
    for (const base::Value& value : all_nodes->GetList()) {
      DCHECK(value.is_dict());
      const base::Value* nodes = value.FindKey("nodes");
      DCHECK(nodes);
      DCHECK(nodes->is_list());
      // Ignore types that are empty, because we expect the root node.
      if (nodes->GetList().empty()) {
        continue;
      }
      const base::Value* model_type = value.FindKey("type");
      DCHECK(model_type);
      DCHECK(model_type->is_string());
      if (type == ModelTypeFromString(model_type->GetString())) {
        return true;
      }
    }
    return false;
  }

 protected:
  void SetupTest(bool all_types_enabled) {
    ASSERT_TRUE(SetupClients());
    sync_prefs_ = std::make_unique<SyncPrefs>(GetProfile(0)->GetPrefs());
    if (all_types_enabled) {
      ASSERT_TRUE(GetClient(0)->SetupSync());
    } else {
      ASSERT_TRUE(GetClient(0)->SetupSync(ModelTypeSet()));
    }

    registered_types_ = GetSyncService(0)->GetRegisteredDataTypes();
    selectable_types_ = UserSelectableTypes();
    multi_grouped_types_ = MultiGroupTypes(*sync_prefs_, registered_types_);
  }

  ModelTypeSet ResolveGroup(ModelType type) {
    return Difference(sync_prefs_->ResolvePrefGroups(
                          registered_types_, ModelTypeSet(type),
                          unified_consent::IsUnifiedConsentFeatureEnabled()),
                      ProxyTypes());
  }

  ModelTypeSet WithoutMultiTypes(const ModelTypeSet& input) {
    return Difference(input, multi_grouped_types_);
  }

  void TearDownOnMainThread() override {
    // Has to be done before user prefs are destroyed.
    sync_prefs_.reset();
    SyncTest::TearDownOnMainThread();
  }

  std::unique_ptr<SyncPrefs> sync_prefs_;
  ModelTypeSet registered_types_;
  ModelTypeSet selectable_types_;
  ModelTypeSet multi_grouped_types_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnableDisableSingleClientTest);
};

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (ModelType st : selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(st);
    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelType sgt : single_grouped_types) {
      ASSERT_FALSE(ModelTypeExists(sgt)) << " for " << ModelTypeToString(st);
    }

    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(st));

    for (ModelType gt : grouped_types) {
      EXPECT_TRUE(ModelTypeExists(gt)) << " for " << ModelTypeToString(st);
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, DisableOneAtATime) {
  // Setup sync with no disabled types.
  SetupTest(/*all_types_enabled=*/true);

  for (ModelType st : selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(st);
    for (ModelType gt : grouped_types) {
      ASSERT_TRUE(ModelTypeExists(gt)) << " for " << ModelTypeToString(st);
    }

    EXPECT_TRUE(GetClient(0)->DisableSyncForDatatype(st));

    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelType sgt : single_grouped_types) {
      EXPECT_FALSE(ModelTypeExists(sgt)) << " for " << ModelTypeToString(st);
    }
  }

  // Lastly make sure that all the multi grouped times are all gone, since we
  // did not check these after disabling inside the above loop.
  for (ModelType mgt : multi_grouped_types_) {
    EXPECT_FALSE(ModelTypeExists(mgt)) << " for " << ModelTypeToString(mgt);
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       FastEnableDisableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (ModelType st : selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(st);
    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelType sgt : single_grouped_types) {
      ASSERT_FALSE(ModelTypeExists(sgt)) << " for " << ModelTypeToString(st);
    }

    // Enable and then disable immediately afterwards, before the datatype has
    // had the chance to finish startup (which usually involves task posting).
    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(st));
    EXPECT_TRUE(GetClient(0)->DisableSyncForDatatype(st));

    for (ModelType sgt : single_grouped_types) {
      EXPECT_FALSE(ModelTypeExists(sgt)) << " for " << ModelTypeToString(st);
    }
  }

  // Lastly make sure that all the multi grouped times are all gone, since we
  // did not check these after disabling inside the above loop.
  for (ModelType mgt : multi_grouped_types_) {
    EXPECT_FALSE(ModelTypeExists(mgt)) << " for " << ModelTypeToString(mgt);
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       FastDisableEnableOneAtATime) {
  // Setup sync with no disabled types.
  SetupTest(/*all_types_enabled=*/true);

  for (ModelType st : selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(st);
    for (ModelType gt : grouped_types) {
      ASSERT_TRUE(ModelTypeExists(gt)) << " for " << ModelTypeToString(st);
    }

    // Disable and then reenable immediately afterwards, before the datatype has
    // had the chance to stop fully (which usually involves task posting).
    EXPECT_TRUE(GetClient(0)->DisableSyncForDatatype(st));
    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(st));

    for (ModelType gt : grouped_types) {
      EXPECT_TRUE(ModelTypeExists(gt)) << " for " << ModelTypeToString(st);
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       FastEnableDisableEnableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (ModelType st : selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(st);
    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelType sgt : single_grouped_types) {
      ASSERT_FALSE(ModelTypeExists(sgt)) << " for " << ModelTypeToString(st);
    }

    // Fast enable-disable-enable sequence, before the datatype has had the
    // chance to transition fully across states (usually involves task posting).
    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(st));
    EXPECT_TRUE(GetClient(0)->DisableSyncForDatatype(st));
    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(st));

    for (ModelType sgt : single_grouped_types) {
      EXPECT_TRUE(ModelTypeExists(sgt)) << " for " << ModelTypeToString(st);
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableDisable) {
  SetupTest(/*all_types_enabled=*/false);

  // Enable all, and then disable immediately afterwards, before datatypes
  // have had the chance to finish startup (which usually involves task
  // posting).
  GetClient(0)->EnableSyncForAllDatatypes();
  GetClient(0)->DisableSyncForAllDatatypes();

  for (ModelType st : selectable_types_) {
    EXPECT_FALSE(ModelTypeExists(st)) << " for " << ModelTypeToString(st);
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, FastEnableDisableEnable) {
  SetupTest(/*all_types_enabled=*/false);

  // Enable all, and then disable+reenable immediately afterwards, before
  // datatypes have had the chance to finish startup (which usually involves
  // task posting).
  GetClient(0)->EnableSyncForAllDatatypes();
  GetClient(0)->DisableSyncForAllDatatypes();
  GetClient(0)->EnableSyncForAllDatatypes();

  // Proxy types don't really run.
  const ModelTypeSet non_proxy_types =
      Difference(selectable_types_, ProxyTypes());

  for (ModelType type : non_proxy_types) {
    EXPECT_TRUE(ModelTypeExists(type)) << " for " << ModelTypeToString(type);
  }
}

}  // namespace

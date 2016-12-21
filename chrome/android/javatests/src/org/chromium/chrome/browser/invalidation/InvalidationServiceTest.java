// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.content.Intent;
import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;

import com.google.ipc.invalidation.external.client.types.ObjectId;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.invalidation.IntentSavingContext;
import org.chromium.components.invalidation.InvalidationService;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.ModelTypeHelper;
import org.chromium.components.sync.notifier.InvalidationIntentProtocol;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.util.Set;

/**
 * Tests for {@link InvalidationService}.
 */
public class InvalidationServiceTest extends NativeLibraryTestBase {
    private IntentSavingContext mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        mContext = new IntentSavingContext(getInstrumentation().getTargetContext());
        // We don't want to use the system content resolver, so we override it.
        MockSyncContentResolverDelegate delegate = new MockSyncContentResolverDelegate();
        // Android master sync can safely always be on.
        delegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, delegate);
    }

    @SmallTest
    @UiThreadTest
    @Feature({"Sync"})
    public void testSetRegisteredObjectIds() {
        InvalidationService service = InvalidationServiceFactory.getForTest(mContext);
        ObjectId bookmark = ModelTypeHelper.toObjectId(ModelType.BOOKMARKS);
        service.setRegisteredObjectIds(new int[] {1, 2, bookmark.getSource()},
                                          new String[] {"a", "b", new String(bookmark.getName())});
        assertEquals(1, mContext.getNumStartedIntents());

        // Validate destination.
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(InvalidationIntentProtocol.ACTION_REGISTER, intent.getAction());

        // Validate registered object ids. The bookmark object should not be registered since it is
        // a Sync type.
        assertNull(intent.getStringArrayListExtra(
                                InvalidationIntentProtocol.EXTRA_REGISTERED_TYPES));
        Set<ObjectId> objectIds = InvalidationIntentProtocol.getRegisteredObjectIds(intent);
        assertEquals(2, objectIds.size());
        assertTrue(objectIds.contains(ObjectId.newInstance(1, "a".getBytes())));
        assertTrue(objectIds.contains(ObjectId.newInstance(2, "b".getBytes())));
    }

    /**
     * Asserts that {@code intent} is destined for the correct component.
     */
    private static void validateIntentComponent(Intent intent) {
        assertNotNull(intent.getComponent());
        assertEquals(ChromeInvalidationClientService.class.getName(),
                intent.getComponent().getClassName());
    }

}

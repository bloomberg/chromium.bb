// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for {@link Tile}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TileTest {
    private static final String[] URLS = {"https://www.google.com", "https://tellmedadjokes.com"};

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testImportDataWithNewTitle() {
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);
        Tile oldTile = new Tile("oldTitle", URLS[0], "", 0, TileSource.TOP_SITES);

        // importData should report the change, and the new tile should keep its own data.
        assertTrue(tile.importData(oldTile));
        assertThat(tile.getTitle(), equalTo("title"));
    }

    @Test(expected = AssertionError.class)
    public void testImportDataWithNewUrl() {
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);
        Tile oldTile = new Tile("title", URLS[1], "", 0, TileSource.TOP_SITES);

        // Importing from a tile associated to a different URL should crash, as bug detection
        // measure.
        tile.importData(oldTile);
    }

    @Test
    public void testImportDataWithNewWhitelistIconPath() {
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);
        Tile oldTile = new Tile("title", URLS[0], "foobar", 0, TileSource.TOP_SITES);

        // importData should report the change, and the new tile should keep its own data.
        assertTrue(tile.importData(oldTile));
        assertThat(tile.getWhitelistIconPath(), equalTo(""));
    }

    @Test
    public void testImportDataWithNewIndex() {
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);
        Tile oldTile = new Tile("title", URLS[0], "", 1, TileSource.TOP_SITES);

        // importData should report the change, and the new tile should keep its own data.
        assertTrue(tile.importData(oldTile));
        assertThat(tile.getIndex(), equalTo(0));
    }

    @Test
    public void testImportDataWithNewSource() {
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);
        Tile oldTile = new Tile("title", URLS[0], "", 0, TileSource.POPULAR);

        // importData should not report the change since it has no visible impact. The new tile
        // still keeps its own data.
        assertFalse(tile.importData(oldTile));
        assertThat(tile.getSource(), equalTo(TileSource.TOP_SITES));
    }

    @Test
    public void testTileImportWithSameData() {
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);
        Tile oldTile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);

        // No change should be reported
        assertFalse(tile.importData(oldTile));
    }

    @Test
    public void testTileImportWithTransientData() {
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);

        Drawable dummyDrawable = new GradientDrawable();
        Tile oldTile = new Tile("title", URLS[0], "", 0, TileSource.TOP_SITES);
        oldTile.setOfflinePageOfflineId(42L);
        oldTile.setIcon(dummyDrawable);
        oldTile.setType(TileVisualType.ICON_REAL);

        // Old transient data should be copied over to the new tile without flagging the change.
        assertFalse(tile.importData(oldTile));
        assertThat(tile.getOfflinePageOfflineId(), equalTo(42L));
        assertThat(tile.getIcon(), equalTo(dummyDrawable));
        assertThat(tile.getType(), equalTo(TileVisualType.ICON_REAL));
    }
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.SmallTest;
import android.util.Base64;

import org.chromium.chrome.browser.util.StreamUtil;
import org.chromium.chrome.shell.ChromeShellTestBase;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Tests whether TabState can be saved and restored to disk properly. Also checks to see if
 * TabStates from previous versions of Chrome can still be loaded and upgraded.
 *
 * This test suite explicitly stores the tab states in this file to get around an infrastructure bug
 * with setting file permissions. Ideally there wouldn't be any real I/O in this function, but we
 * need it because TabState functions require FileInputStreams.
 */
public class TabStateTest extends ChromeShellTestBase {
    private static final String M18_TAB0 =
            "AAABPLD4wNkAAALk4AIAAAAAAAACAAAAAQAAAB0AAABjaHJvbWU6Ly9uZXd0YWIvI21vc3Rfdmlz"
            + "aXRlZAAAAAAAAAAHAAAATgBlAHcAIAB0AGEAYgAAADQBAAAwAQAACwAAADoAAABjAGgAcgBvAG0A"
            + "ZQA6AC8ALwBuAGUAdwB0AGEAYgAvACMAbQBvAHMAdABfAHYAaQBzAGkAdABlAGQAAAA6AAAAYwBo"
            + "AHIAbwBtAGUAOgAvAC8AbgBlAHcAdABhAGIALwAjAG0AbwBzAHQAXwB2AGkAcwBpAHQAZQBkAAAA"
            + "/////wAAAAD//////////wgAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAD/////AAAAAAgAAAAA"
            + "AAAAAAAAQHuHvz0F1QQAfIe/PQXVBAABAAAAMgAAAP8BPwBvPwFTCGZvbGRlcklkPwFTATM/AVMR"
            + "c2VsZWN0ZWRQYW5lSW5kZXg/AUkCewIAAAAAAAAA//////////8IAAAAAAAAAAAAAEABAAAAAAAA"
            + "AAgAAAAWAAAAaHR0cDovL3d3dy5nb29nbGUuY29tLwAAAAAAAAYAAABHAG8AbwBnAGwAZQDcAAAA"
            + "2AAAAAsAAAAsAAAAaAB0AHQAcAA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC4AYwBvAG0ALwAs"
            + "AAAAaAB0AHQAcAA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC4AYwBvAG0ALwD/////AAAAAP//"
            + "////////CAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAP////8AAAAACAAAAAAAAMCcguc/o8Iq"
            + "PgXVBACkwio+BdUEAAAAAAAAAAAA//////////8IAAAAAAAAwJyC5z8AAAAAAAAAAAgAAAAdAAAA"
            + "Y2hyb21lOi8vbmV3dGFiLyNtb3N0X3Zpc2l0ZWQAAAAAAAAAFgAAAGh0dHA6Ly93d3cuZ29vZ2xl"
            + "LmNvbS8AAAAAAAD/////AAA=";

    private static final String M18_TAB1 =
            "AAABPK1gIpkAAAQYFAQAAAAAAAACAAAAAAAAAB0AAABjaHJvbWU6Ly9uZXd0YWIvI21vc3Rfdmlz"
            + "aXRlZAAAAAAAAAAHAAAATgBlAHcAIAB0AGEAYgAAADQBAAAwAQAACwAAADoAAABjAGgAcgBvAG0A"
            + "ZQA6AC8ALwBuAGUAdwB0AGEAYgAvACMAbQBvAHMAdABfAHYAaQBzAGkAdABlAGQAAAA6AAAAYwBo"
            + "AHIAbwBtAGUAOgAvAC8AbgBlAHcAdABhAGIALwAjAG0AbwBzAHQAXwB2AGkAcwBpAHQAZQBkAAAA"
            + "/////wAAAAD//////////wgAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAD/////AAAAAAgAAAAA"
            + "AAAAAAAAQMYvST4F1QQAxy9JPgXVBAABAAAAMgAAAP8BPwBvPwFTCGZvbGRlcklkPwFTATM/AVMR"
            + "c2VsZWN0ZWRQYW5lSW5kZXg/AUkCewIAAAAAAAAA//////////8IAAAAAAAAAAAAAEABAAAAAAAA"
            + "AAYAAAEVAAAAaHR0cDovL3d3dy5nb29nbGUuY2EvAAAAAAAAAAYAAABHAG8AbwBnAGwAZQAUAgAA"
            + "EAIAAAsAAAAqAAAAaAB0AHQAcAA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC4AYwBhAC8AAAAi"
            + "AAAAaAB0AHQAcAA6AC8ALwBnAG8AbwBnAGwAZQAuAGMAYQAvAAAA/////wAAAAAMAAAARwBvAG8A"
            + "ZwBsAGUA/////wgAAAAAAAAAAAAAAAAAAAACAAAAAQAAAAAAAAD/////AwAAAAYAAABjAHMAaQAA"
            + "ABAAAAB0AGUAeAB0AGEAcgBlAGEA/////wgAAAAAAAAAAAAAQHu+oD4F1QQAfL6gPgXVBAAAAAAA"
            + "AAAAAP//////////CAAAAAAAAAAAAABAAQAAAAEAAAALAAAAFgAAAGEAYgBvAHUAdAA6AGIAbABh"
            + "AG4AawAAABYAAABhAGIAbwB1AHQAOgBiAGwAYQBuAGsAAAAIAAAAdwBnAGoAZgD/////////////"
            + "//8IAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAKgAAAGgAdAB0AHAAOgAvAC8AdwB3AHcALgBn"
            + "AG8AbwBnAGwAZQAuAGMAYQAvAAAAAAAAAAgAAAAAAAAAAADwP32+oD4F1QQAfr6gPgXVBAAAAAAA"
            + "AAAAAP////8qAAAAaAB0AHQAcAA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC4AYwBhAC8AAAAI"
            + "AAAAAAAAAAAA8D8BAAAAAAAAAAEAAAAdAAAAY2hyb21lOi8vbmV3dGFiLyNtb3N0X3Zpc2l0ZWQA"
            + "AAAAAAAAEQAAAGh0dHA6Ly9nb29nbGUuY2EvAAAAAAAAAP////8AAA==";

    private static final String M26_TAB0 =
            "AAABPK2JhPQAAALg3AIAAAAAAAACAAAAAQAAAAAAAAAdAAAAY2hyb21lOi8vbmV3dGFiLyNtb3N0"
            + "X3Zpc2l0ZWQAAAAHAAAATgBlAHcAIAB0AGEAYgAAACQBAAAgAQAADQAAADoAAABjAGgAcgBvAG0A"
            + "ZQA6AC8ALwBuAGUAdwB0AGEAYgAvACMAbQBvAHMAdABfAHYAaQBzAGkAdABlAGQAAAA6AAAAYwBo"
            + "AHIAbwBtAGUAOgAvAC8AbgBlAHcAdABhAGIALwAjAG0AbwBzAHQAXwB2AGkAcwBpAHQAZQBkAAAA"
            + "/////wAAAAD//////////wgAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAD/////AAAAAAgAAAAA"
            + "AAAAAADwP2hSNuEF1QQAaVI24QXVBAABAAAAMgAAAP8CPwBvPwFTCGZvbGRlcklkPwFTATM/AVMR"
            + "c2VsZWN0ZWRQYW5lSW5kZXg/AUkCewIAAAAAAAAA//////////8AAAAABgAAAAAAAAAAAAAAAQAA"
            + "AB0AAABjaHJvbWU6Ly9uZXd0YWIvI21vc3RfdmlzaXRlZAAAAAAAAABaa9YpnDMuAAEAAAAWAAAA"
            + "aHR0cDovL3d3dy5nb29nbGUuY29tLwAABgAAAEcAbwBvAGcAbABlAMQAAADAAAAADQAAACwAAABo"
            + "AHQAdABwADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALgBjAG8AbQAvACQAAABoAHQAdABwADoA"
            + "LwAvAGcAbwBvAGcAbABlAC4AYwBvAG0ALwD/////AAAAAP//////////CAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAABAAAAAAAAAP////8AAAAACAAAAAAAAMCcgtc/XIjK4gXVBABdiMriBdUEAAAAAAAAAAAA"
            + "//////////8AAAAAAQAAAgAAAAAAAAAAAQAAABIAAABodHRwOi8vZ29vZ2xlLmNvbS8AAAAAAABI"
            + "AVIrnDMuAP////8AAA==";

    private static final String M26_TAB1 =
            "AAABPK2J90YAAALs6AIAAAAAAAACAAAAAQAAAAAAAAAdAAAAY2hyb21lOi8vbmV3dGFiLyNtb3N0"
            + "X3Zpc2l0ZWQAAAAHAAAATgBlAHcAIAB0AGEAYgAAACQBAAAgAQAADQAAADoAAABjAGgAcgBvAG0A"
            + "ZQA6AC8ALwBuAGUAdwB0AGEAYgAvACMAbQBvAHMAdABfAHYAaQBzAGkAdABlAGQAAAA6AAAAYwBo"
            + "AHIAbwBtAGUAOgAvAC8AbgBlAHcAdABhAGIALwAjAG0AbwBzAHQAXwB2AGkAcwBpAHQAZQBkAAAA"
            + "/////wAAAAD//////////wgAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAD/////AAAAAAgAAAAA"
            + "AAAAAADwP9eU9OIF1QQA2JT04gXVBAABAAAAMgAAAP8CPwBvPwFTCGZvbGRlcklkPwFTATM/AVMR"
            + "c2VsZWN0ZWRQYW5lSW5kZXg/AUkCewIAAAAAAAAA//////////8AAAAABgAAAAAAAAAAAAAAAQAA"
            + "AB0AAABjaHJvbWU6Ly9uZXd0YWIvI21vc3RfdmlzaXRlZAAAAAAAAADl8oQrnDMuAAEAAAAVAAAA"
            + "aHR0cDovL3d3dy5nb29nbGUuY2EvAAAABgAAAEcAbwBvAGcAbABlAMwAAADIAAAADQAAACoAAABo"
            + "AHQAdABwADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALgBjAGEALwAAACoAAABoAHQAdABwADoA"
            + "LwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALgBjAGEALwAAAP////8AAAAA//////////8IAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAEAAAAAAAAA/////wAAAAAIAAAAAAAAAAAA8D9VtDjjBdUEAFa0OOMF1QQA"
            + "AAAAAAAAAAD//////////wAAAAABAAAAAAAAAAAAAAABAAAAFQAAAGh0dHA6Ly93d3cuZ29vZ2xl"
            + "LmNhLwAAAAAAAAD8oBUsnDMuAP////8AAA==";

    private static final String M38_TAB =
            "AAABSPI9OA8AAALs6AIAAAAAAAACAAAAAQAAACABAAAcAQAAAAAAABcAAABjaHJvbWUtbmF0aXZl"
            + "Oi8vbmV3dGFiLwAHAAAATgBlAHcAIAB0AGEAYgAAAKQAAACgAAAAFQAAAAAAAAAuAAAAYwBoAHIA"
            + "bwBtAGUALQBuAGEAdABpAHYAZQA6AC8ALwBuAGUAdwB0AGEAYgAvAAAA/////wAAAAAAAAAA////"
            + "/wAAAAAIAAAAAAAAAAAA8D8EkSY/8gQFAAWRJj/yBAUABpEmP/IEBQABAAAACAAAAAAAAAAAAPC/"
            + "CAAAAAAAAAAAAPC/AAAAAAAAAAD/////AAAAAAYAAAAAAAAAAAAAAAEAAAAXAAAAY2hyb21lLW5h"
            + "dGl2ZTovL25ld3RhYi8AAAAAAMnUrIeIYy4AAAAAAAAAAAC0AQAAsAEAAAEAAAAUAAAAaHR0cDov"
            + "L3RleHRhcmVhLm9yZy8IAAAAdABlAHgAdABhAHIAZQBhAEABAAA8AQAAFQAAAAAAAAAoAAAAaAB0"
            + "AHQAcAA6AC8ALwB0AGUAeAB0AGEAcgBlAGEALgBvAHIAZwAvAP////8AAAAAAAAAAP////8HAAAA"
            + "YAAAAAoADQA/ACUAIABXAGUAYgBLAGkAdAAgAHMAZQByAGkAYQBsAGkAegBlAGQAIABmAG8AcgBt"
            + "ACAAcwB0AGEAdABlACAAdgBlAHIAcwBpAG8AbgAgADgAIAAKAA0APQAmABAAAABOAG8AIABvAHcA"
            + "bgBlAHIAAgAAADEAAAAAAAAAEAAAAHQAZQB4AHQAYQByAGUAYQACAAAAMQAAAAAAAAAIAAAAAAAA"
            + "AAAAAAAHkSY/8gQFAAiRJj/yBAUABpEmP/IEBQABAAAACAAAAAAAAAAAAAAACAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAD/////AAAAAAEAAAIAAAAAAAAAAAEAAAAUAAAAaHR0cDovL3RleHRhcmVhLm9yZy8A"
            + "AAAANKvVh4hjLgAAAAAAyAAAAP////8AAAAAAAIAAAAAAAAAAwE=";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        clearAppData();
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
    }

    @Override
    public void tearDown() throws Exception {
        TabState.setChannelNameOverrideForTest(null);
        super.tearDown();
    }

    private void doTest(String encodedFile, String url,
            String title, int expectedVersion) throws IOException {
        String filename = "tab_temp";
        File directory = getInstrumentation().getTargetContext().getCacheDir();
        File tabStateFile = new File(directory, filename);
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(tabStateFile);
            outputStream.write(Base64.decode(encodedFile, 0));
        } catch (FileNotFoundException e) {
            assert false : "Failed to create " + filename;
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }

        TabState tabState = TabState.restoreTabState(tabStateFile, false);
        if (!tabStateFile.delete()) {
            assert false : "Failed to delete " + filename;
        }
        assertNotNull(tabState);
        assertEquals(url, tabState.getVirtualUrlFromState());
        assertEquals(title, tabState.getDisplayTitleFromState());
        assertEquals(expectedVersion, tabState.contentsState.version());
    }

    @SmallTest
    public void testLoadM18Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest("stable");
        doTest(M18_TAB0, "http://www.google.com/", "Google", 0);
        doTest(M18_TAB1, "chrome://newtab/#most_visited", "New tab", 0);
    }

    @SmallTest
    public void testLoadM26Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest(null);
        doTest(M26_TAB0, "http://www.google.com/", "Google", 1);
        doTest(M26_TAB1, "http://www.google.ca/", "Google", 1);
    }

    @SmallTest
    public void testLoadM38Tab() throws Exception {
        TabState.setChannelNameOverrideForTest(null);
        doTest(M38_TAB, "http://textarea.org/", "textarea", 2);
    }
}

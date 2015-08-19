// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.util.Base64;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;

/**
 * Creates and fills up a directory with data suitable for testing code related to {@link TabModel}.
 * Used for mocking out the real data directory.
 *
 * Instead of storing the TabState and metadata files in the test data directories, this class
 * stores them as encoded Base64 strings and then writes them out to storage during the test.
 * This gets around an infrastructure bug with setting file permissions.
 */
public class TestTabModelDirectory {
    private static final String TAG = "cr.tabmodel";

    /**
     * Information about an encoded TabState file.  Although the Tab ID is _not_ encoded in the
     * TabState file, it is convenient to store it in this class for testing purposes.
     */
    public static final class TabStateInfo {
        public final int version;
        public final int tabId;
        public final String url;
        public final String title;
        public final String encodedTabState;
        public final String filename;

        TabStateInfo(int version, int tabId, String url, String title, String encodedTabState) {
            this.version = version;
            this.tabId = tabId;
            this.url = url;
            this.title = title;
            this.encodedTabState = encodedTabState;
            this.filename = "tab" + tabId;
        }
    };

    public static final TabStateInfo M18_NTP = new TabStateInfo(
            0,
            0,
            "chrome-native://newtab/",
            "New tab",
            "AAABPK1gIpkAAAQYFAQAAAAAAAACAAAAAAAAAB0AAABjaHJvbWU6Ly9uZXd0YWIvI21vc3RfdmlzaXRlZAAAAA"
            + "AAAAAHAAAATgBlAHcAIAB0AGEAYgAAADQBAAAwAQAACwAAADoAAABjAGgAcgBvAG0AZQA6AC8ALwBuAGUAdw"
            + "B0AGEAYgAvACMAbQBvAHMAdABfAHYAaQBzAGkAdABlAGQAAAA6AAAAYwBoAHIAbwBtAGUAOgAvAC8AbgBlAH"
            + "cAdABhAGIALwAjAG0AbwBzAHQAXwB2AGkAcwBpAHQAZQBkAAAA/////wAAAAD//////////wgAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAQAAAAAAAAD/////AAAAAAgAAAAAAAAAAAAAQMYvST4F1QQAxy9JPgXVBAABAAAAMgAAAP"
            + "8BPwBvPwFTCGZvbGRlcklkPwFTATM/AVMRc2VsZWN0ZWRQYW5lSW5kZXg/AUkCewIAAAAAAAAA//////////"
            + "8IAAAAAAAAAAAAAEABAAAAAAAAAAYAAAEVAAAAaHR0cDovL3d3dy5nb29nbGUuY2EvAAAAAAAAAAYAAABHAG"
            + "8AbwBnAGwAZQAUAgAAEAIAAAsAAAAqAAAAaAB0AHQAcAA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC4AYw"
            + "BhAC8AAAAiAAAAaAB0AHQAcAA6AC8ALwBnAG8AbwBnAGwAZQAuAGMAYQAvAAAA/////wAAAAAMAAAARwBvAG"
            + "8AZwBsAGUA/////wgAAAAAAAAAAAAAAAAAAAACAAAAAQAAAAAAAAD/////AwAAAAYAAABjAHMAaQAAABAAAA"
            + "B0AGUAeAB0AGEAcgBlAGEA/////wgAAAAAAAAAAAAAQHu+oD4F1QQAfL6gPgXVBAAAAAAAAAAAAP////////"
            + "//CAAAAAAAAAAAAABAAQAAAAEAAAALAAAAFgAAAGEAYgBvAHUAdAA6AGIAbABhAG4AawAAABYAAABhAGIAbw"
            + "B1AHQAOgBiAGwAYQBuAGsAAAAIAAAAdwBnAGoAZgD///////////////8IAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAKgAAAGgAdAB0AHAAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQAuAGMAYQAvAAAAAAAAAAgAAA"
            + "AAAAAAAADwP32+oD4F1QQAfr6gPgXVBAAAAAAAAAAAAP////8qAAAAaAB0AHQAcAA6AC8ALwB3AHcAdwAuAG"
            + "cAbwBvAGcAbABlAC4AYwBhAC8AAAAIAAAAAAAAAAAA8D8BAAAAAAAAAAEAAAAdAAAAY2hyb21lOi8vbmV3dG"
            + "FiLyNtb3N0X3Zpc2l0ZWQAAAAAAAAAEQAAAGh0dHA6Ly9nb29nbGUuY2EvAAAAAAAAAP////8AAA==");

    public static final TabStateInfo M18_GOOGLE_COM = new TabStateInfo(
            0,
            1,
            "http://www.google.com/",
            "Google",
            "AAABPLD4wNkAAALk4AIAAAAAAAACAAAAAQAAAB0AAABjaHJvbWU6Ly9uZXd0YWIvI21vc3RfdmlzaXRlZAAAAA"
            + "AAAAAHAAAATgBlAHcAIAB0AGEAYgAAADQBAAAwAQAACwAAADoAAABjAGgAcgBvAG0AZQA6AC8ALwBuAGUAdw"
            + "B0AGEAYgAvACMAbQBvAHMAdABfAHYAaQBzAGkAdABlAGQAAAA6AAAAYwBoAHIAbwBtAGUAOgAvAC8AbgBlAH"
            + "cAdABhAGIALwAjAG0AbwBzAHQAXwB2AGkAcwBpAHQAZQBkAAAA/////wAAAAD//////////wgAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAQAAAAAAAAD/////AAAAAAgAAAAAAAAAAAAAQHuHvz0F1QQAfIe/PQXVBAABAAAAMgAAAP"
            + "8BPwBvPwFTCGZvbGRlcklkPwFTATM/AVMRc2VsZWN0ZWRQYW5lSW5kZXg/AUkCewIAAAAAAAAA//////////"
            + "8IAAAAAAAAAAAAAEABAAAAAAAAAAgAAAAWAAAAaHR0cDovL3d3dy5nb29nbGUuY29tLwAAAAAAAAYAAABHAG"
            + "8AbwBnAGwAZQDcAAAA2AAAAAsAAAAsAAAAaAB0AHQAcAA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC4AYw"
            + "BvAG0ALwAsAAAAaAB0AHQAcAA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC4AYwBvAG0ALwD/////AAAAAP"
            + "//////////CAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAP////8AAAAACAAAAAAAAMCcguc/o8IqPgXVBA"
            + "Ckwio+BdUEAAAAAAAAAAAA//////////8IAAAAAAAAwJyC5z8AAAAAAAAAAAgAAAAdAAAAY2hyb21lOi8vbm"
            + "V3dGFiLyNtb3N0X3Zpc2l0ZWQAAAAAAAAAFgAAAGh0dHA6Ly93d3cuZ29vZ2xlLmNvbS8AAAAAAAD/////AA"
            + "A=");

    public static final TabStateInfo M26_GOOGLE_COM = new TabStateInfo(
            1,
            2,
            "http://www.google.com/",
            "Google",
            "AAABPK2JhPQAAALg3AIAAAAAAAACAAAAAQAAAAAAAAAdAAAAY2hyb21lOi8vbmV3dGFiLyNtb3N0X3Zpc2l0ZW"
            + "QAAAAHAAAATgBlAHcAIAB0AGEAYgAAACQBAAAgAQAADQAAADoAAABjAGgAcgBvAG0AZQA6AC8ALwBuAGUAdw"
            + "B0AGEAYgAvACMAbQBvAHMAdABfAHYAaQBzAGkAdABlAGQAAAA6AAAAYwBoAHIAbwBtAGUAOgAvAC8AbgBlAH"
            + "cAdABhAGIALwAjAG0AbwBzAHQAXwB2AGkAcwBpAHQAZQBkAAAA/////wAAAAD//////////wgAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAQAAAAAAAAD/////AAAAAAgAAAAAAAAAAADwP2hSNuEF1QQAaVI24QXVBAABAAAAMgAAAP"
            + "8CPwBvPwFTCGZvbGRlcklkPwFTATM/AVMRc2VsZWN0ZWRQYW5lSW5kZXg/AUkCewIAAAAAAAAA//////////"
            + "8AAAAABgAAAAAAAAAAAAAAAQAAAB0AAABjaHJvbWU6Ly9uZXd0YWIvI21vc3RfdmlzaXRlZAAAAAAAAABaa9"
            + "YpnDMuAAEAAAAWAAAAaHR0cDovL3d3dy5nb29nbGUuY29tLwAABgAAAEcAbwBvAGcAbABlAMQAAADAAAAADQ"
            + "AAACwAAABoAHQAdABwADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALgBjAG8AbQAvACQAAABoAHQAdABwAD"
            + "oALwAvAGcAbwBvAGcAbABlAC4AYwBvAG0ALwD/////AAAAAP//////////CAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "ABAAAAAAAAAP////8AAAAACAAAAAAAAMCcgtc/XIjK4gXVBABdiMriBdUEAAAAAAAAAAAA//////////8AAA"
            + "AAAQAAAgAAAAAAAAAAAQAAABIAAABodHRwOi8vZ29vZ2xlLmNvbS8AAAAAAABIAVIrnDMuAP////8AAA==");

    public static final TabStateInfo M26_GOOGLE_CA = new TabStateInfo(
            1,
            3,
            "http://www.google.ca/",
            "Google",
            "AAABPK2J90YAAALs6AIAAAAAAAACAAAAAQAAAAAAAAAdAAAAY2hyb21lOi8vbmV3dGFiLyNtb3N0X3Zpc2l0ZW"
            + "QAAAAHAAAATgBlAHcAIAB0AGEAYgAAACQBAAAgAQAADQAAADoAAABjAGgAcgBvAG0AZQA6AC8ALwBuAGUAdw"
            + "B0AGEAYgAvACMAbQBvAHMAdABfAHYAaQBzAGkAdABlAGQAAAA6AAAAYwBoAHIAbwBtAGUAOgAvAC8AbgBlAH"
            + "cAdABhAGIALwAjAG0AbwBzAHQAXwB2AGkAcwBpAHQAZQBkAAAA/////wAAAAD//////////wgAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAQAAAAAAAAD/////AAAAAAgAAAAAAAAAAADwP9eU9OIF1QQA2JT04gXVBAABAAAAMgAAAP"
            + "8CPwBvPwFTCGZvbGRlcklkPwFTATM/AVMRc2VsZWN0ZWRQYW5lSW5kZXg/AUkCewIAAAAAAAAA//////////"
            + "8AAAAABgAAAAAAAAAAAAAAAQAAAB0AAABjaHJvbWU6Ly9uZXd0YWIvI21vc3RfdmlzaXRlZAAAAAAAAADl8o"
            + "QrnDMuAAEAAAAVAAAAaHR0cDovL3d3dy5nb29nbGUuY2EvAAAABgAAAEcAbwBvAGcAbABlAMwAAADIAAAADQ"
            + "AAACoAAABoAHQAdABwADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALgBjAGEALwAAACoAAABoAHQAdABwAD"
            + "oALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALgBjAGEALwAAAP////8AAAAA//////////8IAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAEAAAAAAAAA/////wAAAAAIAAAAAAAAAAAA8D9VtDjjBdUEAFa0OOMF1QQAAAAAAAAAAAD///"
            + "///////wAAAAABAAAAAAAAAAAAAAABAAAAFQAAAGh0dHA6Ly93d3cuZ29vZ2xlLmNhLwAAAAAAAAD8oBUsnD"
            + "MuAP////8AAA==");

    public static final TabStateInfo V2_BAIDU = new TabStateInfo(
            2,
            4,
            "http://www.baidu.com/",
            "百度一下",
            "AAABTbBCEBcAAAFkYAEAAAAAAAABAAAAAAAAAFABAABMAQAAAAAAABUAAABodHRwOi8vd3d3LmJhaWR1LmNvbS"
            + "8AAAAEAAAAfnamXgBOC07IAAAAxAAAABYAAAAAAAAAKgAAAGgAdAB0AHAAOgAvAC8AdwB3AHcALgBiAGEAaQ"
            + "BkAHUALgBjAG8AbQAvAAAA/////wAAAAAAAAAAIgAAAGgAdAB0AHAAOgAvAC8AYgBhAGkAZAB1AC4AYwBvAG"
            + "0ALwAAAAAAAAAIAAAAAAAAwJyC1z9MWRSCeBcFAE1ZFIJ4FwUAS1kUgngXBQABAAAACAAAAAAAAAAAAAAACA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/////wAAAAAAAAAIAAAAABEAAABodHRwOi8vYmFpZHUuY29tLwAAAA"
            + "EAAAARAAAAaHR0cDovL2JhaWR1LmNvbS8AAAAAAAAA7Nyiyg52LgAAAAAAyAAAAAEAAAD/////AAAAAAACAA"
            + "AAAAAAAAAA");

    public static final TabStateInfo V2_DUCK_DUCK_GO = new TabStateInfo(
            2,
            5,
            "https://duckduckgo.com/",
            "DuckDuckGo",
            "AAABTbBCExUAAAFAPAEAAAAAAAABAAAAAAAAACwBAAAoAQAAAAAAABcAAABodHRwczovL2R1Y2tkdWNrZ28uY2"
            + "9tLwAKAAAARAB1AGMAawBEAHUAYwBrAEcAbwCoAAAApAAAABYAAAAAAAAALgAAAGgAdAB0AHAAcwA6AC8ALw"
            + "BkAHUAYwBrAGQAdQBjAGsAZwBvAC4AYwBvAG0ALwAAAP////8AAAAAAAAAAP////8AAAAACAAAAAAAAAAAAA"
            + "AAlVAhgngXBQCWUCGCeBcFAJdQIYJ4FwUAAQAAAAgAAAAAAAAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAP////8AAAAAAAAACAAAAAAAAAAAAQAAABYAAABodHRwOi8vZHVja2R1Y2tnby5jb20vAAAAAAAAmoGoyg"
            + "52LgAAAAAAyAAAAAEAAAD/////AAAAAAACAAAAAAAAAAAA");

    public static final TabStateInfo V2_HAARETZ = new TabStateInfo(
            2,
            6,
            "http://www.haaretz.co.il/",
            "חדשות, ידיעות מהארץ והעולם - עיתון הארץ",
            "AAABTbBhcJQAAAD49AAAAAAAAAABAAAAAAAAAOQAAADgAAAAAAAAABkAAABodHRwOi8vd3d3LmhhYXJldHouY2"
            + "8uaWwvAAAAJwAAANcF0wXpBdUF6gUsACAA2QXTBdkF4gXVBeoFIADeBdQF0AXoBeUFIADVBdQF4gXVBdwF3Q"
            + "UgAC0AIADiBdkF6gXVBd8FIADUBdAF6AXlBQAAAAAAAAAAAAgAAAAAGQAAAGh0dHA6Ly93d3cuaGFhcmV0ei"
            + "5jby5pbC8AAAABAAAAGQAAAGh0dHA6Ly93d3cuaGFhcmV0ei5jby5pbC8AAAAAAAAAJ7hFRQ92LgAAAAAAyA"
            + "AAAAEAAAD/////AAAAAAACAAAAAAAAAAAA");

    public static final TabStateInfo V2_TEXTAREA = new TabStateInfo(
            2,
            7,
            "http://textarea.org/",
            "textarea",
            "AAABSPI9OA8AAALs6AIAAAAAAAACAAAAAQAAACABAAAcAQAAAAAAABcAAABjaHJvbWUtbmF0aXZlOi8vbmV3dG"
            + "FiLwAHAAAATgBlAHcAIAB0AGEAYgAAAKQAAACgAAAAFQAAAAAAAAAuAAAAYwBoAHIAbwBtAGUALQBuAGEAdA"
            + "BpAHYAZQA6AC8ALwBuAGUAdwB0AGEAYgAvAAAA/////wAAAAAAAAAA/////wAAAAAIAAAAAAAAAAAA8D8EkS"
            + "Y/8gQFAAWRJj/yBAUABpEmP/IEBQABAAAACAAAAAAAAAAAAPC/CAAAAAAAAAAAAPC/AAAAAAAAAAD/////AA"
            + "AAAAYAAAAAAAAAAAAAAAEAAAAXAAAAY2hyb21lLW5hdGl2ZTovL25ld3RhYi8AAAAAAMnUrIeIYy4AAAAAAA"
            + "AAAAC0AQAAsAEAAAEAAAAUAAAAaHR0cDovL3RleHRhcmVhLm9yZy8IAAAAdABlAHgAdABhAHIAZQBhAEABAA"
            + "A8AQAAFQAAAAAAAAAoAAAAaAB0AHQAcAA6AC8ALwB0AGUAeAB0AGEAcgBlAGEALgBvAHIAZwAvAP////8AAA"
            + "AAAAAAAP////8HAAAAYAAAAAoADQA/ACUAIABXAGUAYgBLAGkAdAAgAHMAZQByAGkAYQBsAGkAegBlAGQAIA"
            + "BmAG8AcgBtACAAcwB0AGEAdABlACAAdgBlAHIAcwBpAG8AbgAgADgAIAAKAA0APQAmABAAAABOAG8AIABvAH"
            + "cAbgBlAHIAAgAAADEAAAAAAAAAEAAAAHQAZQB4AHQAYQByAGUAYQACAAAAMQAAAAAAAAAIAAAAAAAAAAAAAA"
            + "AHkSY/8gQFAAiRJj/yBAUABpEmP/IEBQABAAAACAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAD///"
            + "//AAAAAAEAAAIAAAAAAAAAAAEAAAAUAAAAaHR0cDovL3RleHRhcmVhLm9yZy8AAAAANKvVh4hjLgAAAAAAyA"
            + "AAAP////8AAAAAAAIAAAAAAAAAAwE=");

    /**
     * Tab model metadata file containing information about multiple tabs, with Baidu selected.
     * Ideally we'd have the M18 NTP in here, too, but it's difficult to get Chrome to visit that
     * URL now that the page is gone.
     */
    private static final String TAB_MODEL_METADATA_V4 =
            "AAAABAAAAAf/////AAAAAwAAAAEAFmh0dHA6Ly93d3cuZ29vZ2xlLmNvbS8AAAACABZodHRw"
            + "Oi8vd3d3Lmdvb2dsZS5jb20vAAAAAwAVaHR0cDovL3d3dy5nb29nbGUuY2EvAAAABAAVaHR0"
            + "cDovL3d3dy5iYWlkdS5jb20vAAAABQAXaHR0cHM6Ly9kdWNrZHVja2dvLmNvbS8AAAAGABlo"
            + "dHRwOi8vd3d3LmhhYXJldHouY28uaWwvAAAABwAUaHR0cDovL3RleHRhcmVhLm9yZy8=";
    public static final int TAB_MODEL_METADATA_V4_SELECTED_ID = 4;
    static final TabStateInfo[] TAB_MODEL_METADATA_V4_CONTENTS = {
        M18_GOOGLE_COM, M26_GOOGLE_COM, M26_GOOGLE_CA, V2_BAIDU, V2_DUCK_DUCK_GO, V2_HAARETZ,
        V2_TEXTAREA
    };

    private File mTestingDirectory;

    /**
     * Creates a temporary directory that stores {@link TabState} files and the metadata required to
     * restore the {@link TabModel}s from the {@link TabPersistentStore}.
     *
     * @param context Context to use.
     * @param baseDirectoryName Name of the directory to store test data in.
     * @param subdirectoryName Subdirectory of the base directory.  May be null, in which case the
     *                         baseDirectoryName will store the data.  This mocks out how the
     *                         ChromeTabbedActivity instances all store their data in different
     *                         subdirectories of the base data directory.
     */
    public TestTabModelDirectory(
            Context context, String baseDirectoryName, String subdirectoryName) throws Exception {
        mTestingDirectory = new File(context.getCacheDir(), baseDirectoryName);
        if (mTestingDirectory.exists()) recursivelyDelete(mTestingDirectory);
        if (!mTestingDirectory.mkdirs()) {
            Log.e(TAG, "Failed to create: " + mTestingDirectory.getName());
        }

        // Create the subdirectory.
        File dataDirectory = mTestingDirectory;
        if (subdirectoryName != null) {
            dataDirectory = new File(mTestingDirectory, subdirectoryName);
            if (!dataDirectory.exists() && !dataDirectory.mkdirs()) {
                Log.e(TAG, "Failed to create subdirectory: " + dataDirectory.getName());
            }
        }

        // Fill the subdirectory with mocked pre-generated TabState files and metadata for the
        // TabPersistentStore.
        writeFile(dataDirectory, "tab_state", TAB_MODEL_METADATA_V4);
        writeFile(dataDirectory, M18_NTP.filename, M18_NTP.encodedTabState);
        writeFile(dataDirectory, M26_GOOGLE_COM.filename, M26_GOOGLE_COM.encodedTabState);
        writeFile(dataDirectory, M18_GOOGLE_COM.filename, M18_GOOGLE_COM.encodedTabState);
        writeFile(dataDirectory, M26_GOOGLE_CA.filename, M26_GOOGLE_CA.encodedTabState);
        writeFile(dataDirectory, V2_BAIDU.filename, V2_BAIDU.encodedTabState);
        writeFile(dataDirectory, V2_DUCK_DUCK_GO.filename, V2_DUCK_DUCK_GO.encodedTabState);
        writeFile(dataDirectory, V2_HAARETZ.filename, V2_HAARETZ.encodedTabState);
        writeFile(dataDirectory, V2_TEXTAREA.filename, V2_TEXTAREA.encodedTabState);
    }

    /** Nukes all the testing data. */
    public void tearDown() throws Exception {
        recursivelyDelete(mTestingDirectory);
    }

    /** Returns the base data directory. */
    public File getBaseDirectory() {
        return mTestingDirectory;
    }

    private void writeFile(File directory, String filename, String data) throws Exception {
        File file = new File(directory, filename);
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(file);
            outputStream.write(Base64.decode(data, 0));
        } catch (FileNotFoundException e) {
            assert false : "Failed to create " + filename;
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }

    private void recursivelyDelete(File currentFile) throws Exception {
        if (currentFile.isDirectory()) {
            File[] files = currentFile.listFiles();
            if (files != null) {
                for (File file : files) {
                    recursivelyDelete(file);
                }
            }
        }

        if (!currentFile.delete()) Log.e(TAG, "Failed to delete: " + currentFile);
    }
}

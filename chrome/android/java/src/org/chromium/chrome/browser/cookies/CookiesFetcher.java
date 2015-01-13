// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cookies;

import android.os.AsyncTask;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.ImportantFileWriterAndroid;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content.browser.crypto.CipherFactory;
import org.chromium.content.common.CleanupReference;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;

/**
 * Responsible for fetching, (de)serializing and restoring cookies between the
 * CookieJar and an encrypted file storage.
 */
public class CookiesFetcher {
    /** The default file name for the encrypted cookies storage. */
    public static final String DEFAULT_COOKIE_FILE_NAME = "COOKIES.DAT";

    public static final String TAG = "CookiesFetcher";

    // To make sure the current decipher key matches the one from
    // before. Otherwise, we end up restoring some garbage data on a stale
    // file.
    // TODO(acleung): May be use real cryptographic integrity checks on the
    // whole file instead.
    private static final String MAGIC_STRING = "c0Ok135";

    // Full path of the file that we are fetching / loading
    // cookies from.
    private final String mFileName;

    private final long mNativeCookiesFetcher;
    private final CleanupReference mCleanupReference;

    /**
     * Creates a new fetcher that can use to fetch cookies from cookie jar
     * or from a file.
     *
     * The lifetime of this object is handled internally. Callers only call
     * the public static methods which construct a CookiesFetcher object.
     * It remains alive only during the static call or when it is still
     * waiting for a callback to be invoked. In the latter case, the native
     * counter part will hold a strong reference to this Java class so the GC
     * would not collect it until the callback has been invoked.
     */
    private CookiesFetcher(String filename) {
        mNativeCookiesFetcher = nativeInit();
        mFileName = filename;
        mCleanupReference = new CleanupReference(this,
                new DestroyRunnable(mNativeCookiesFetcher));
    }

    /**
     * Asynchronously fetches cookies from the incognito profile and saving
     * it to file specified in the constructor.
     *
     * @param filename Full path of the file.
     */
    public static void fetchFromCookieJar(String filename) {
        try {
            new CookiesFetcher(filename).fetchFromCookieJarInternal();
        } catch (RuntimeException e) {
            e.printStackTrace();
        }
    }

    private void fetchFromCookieJarInternal() {
        nativeFetchFromCookieJar(mNativeCookiesFetcher);
    }

    /**
     * Synchronously fetches cookies from the file specified and populating
     * the incognito profile with it.
     *
     * @param filename Full path of the file.
     */
    public static void restoreToCookieJar(String filename) {
        try {
            CookiesFetcher fetcher = new CookiesFetcher(filename);
            if (deleteCookieJarIfNecessary(filename)) return;
            fetcher.restoreToCookieJarInternal();
        } catch (RuntimeException e) {
            e.printStackTrace();
        }
    }

    /**
     * Ensure the incognito cookies are deleted when the incognito profile is gone.
     *
     * @param filename Full path of the file.
     * @return Whether or not the cookies were deleted.
     */
    public static boolean deleteCookieJarIfNecessary(String filename) {
        try {
            CookiesFetcher fetcher = new CookiesFetcher(filename);
            if (Profile.getLastUsedProfile().hasOffTheRecordProfile()) return false;
            fetcher.scheduleDeleteCookiesFile();
        } catch (RuntimeException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    private void restoreToCookieJarInternal() {
        // Use an AsyncTask to read the cookies from disk to avoid strict
        // mode violations.
        new AsyncTask<Void, Void, List<CanonicalCookie>>() {
            @Override
            protected List<CanonicalCookie> doInBackground(Void... voids) {
                ArrayList<CanonicalCookie> cookies = new ArrayList<CanonicalCookie>();
                DataInputStream in = null;
                try {
                    Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.DECRYPT_MODE);
                    if (cipher == null) {
                        // Something is wrong. Can't encrypt, don't restore cookies.
                        return cookies;
                    }
                    File fileIn = new File(mFileName);
                    if (!fileIn.exists()) return cookies; // Nothing to read

                    FileInputStream streamIn = new FileInputStream(fileIn);
                    in = new DataInputStream(new CipherInputStream(streamIn, cipher));
                    String check = in.readUTF();
                    if (!MAGIC_STRING.equals(check)) {
                        // Stale cookie file. Chrome might have crashed before it
                        // can delete the old file.
                        return cookies;
                    }
                    try {
                        while (true) {
                            CanonicalCookie cookie = CanonicalCookie.createFromStream(in);
                            cookies.add(cookie);
                        }
                    } catch (EOFException ignored) {
                      // We are done.
                    }

                    // The Cookie File should not be restored again. It'll be overwritten
                    // on the next onPause.
                    scheduleDeleteCookiesFile();

                } catch (IOException e) {
                    Log.w(TAG, "IOException during Cookie Restore");
                } catch (Throwable t) {
                    Log.w(TAG, "Error restoring cookies.", t);
                } finally {
                    try {
                        if (in != null) in.close();
                    } catch (IOException e) {
                        Log.w(TAG, "IOException during Cooke Restore");
                    } catch (Throwable t) {
                        Log.w(TAG, "Error restoring cookies.", t);
                    }
                }
                return cookies;
            }

            @Override
            protected void onPostExecute(List<CanonicalCookie> cookies) {
                // onPostExecute is back on the UI thread. We can only access
                // cookies and profiles on the UI thread.
                for (CanonicalCookie cookie : cookies) {
                    nativeRestoreToCookieJar(mNativeCookiesFetcher,
                            cookie.getUrl(),
                            cookie.getName(),
                            cookie.getValue(),
                            cookie.getDomain(),
                            cookie.getPath(),
                            cookie.getCreationDate(),
                            cookie.getExpirationDate(),
                            cookie.getLastAccessDate(),
                            cookie.isSecure(),
                            cookie.isHttpOnly(),
                            cookie.getPriority()
                    );
                }
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @CalledByNative
    private CanonicalCookie createCookie(String url, String name, String value, String domain,
            String path, long creation, long expiration, long lastAccess, boolean secure,
            boolean httpOnly, int priority) {
        return new CanonicalCookie(url, name, value, domain, path, creation, expiration, lastAccess,
                secure, httpOnly, priority);
    }

    @CalledByNative
    private void onCookieFetchFinished(final CanonicalCookie[] cookies) {
        // Cookies fetching requires operations with the profile and must be
        // done in the main thread. Once that is done, do the save to disk
        // part in {@link AsyncTask} to avoid strict mode violations.
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... voids) {
                saveFetchedCookiesToDisk(cookies);
                return null;
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private void saveFetchedCookiesToDisk(CanonicalCookie[] cookies) {
        DataOutputStream out = null;
        try {
            Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.ENCRYPT_MODE);
            if (cipher == null) {
                // Something is wrong. Can't encrypt, don't save cookies.
                return;
            }

            ByteArrayOutputStream byteOut = new ByteArrayOutputStream();
            CipherOutputStream cipherOut =
                    new CipherOutputStream(byteOut, cipher);
            out = new DataOutputStream(cipherOut);

            out.writeUTF(MAGIC_STRING);
            for (CanonicalCookie cookie : cookies) {
                cookie.saveToStream(out);
            }
            out.close();
            ImportantFileWriterAndroid.writeFileAtomically(
                    mFileName, byteOut.toByteArray());
            out = null;
        } catch (IOException e) {
            Log.w(TAG, "IOException during Cookie Fetch");
        } catch (Throwable t) {
            Log.w(TAG, "Error storing cookies.", t);
        } finally {
            try {
                if (out != null) out.close();
            } catch (IOException e) {
                Log.w(TAG, "IOException during Cookie Fetch");
            }
        }
    }

    /**
     * Delete the cookies file. This happens when we detect that all incognito
     * tabs have been closed.
     */
    private void scheduleDeleteCookiesFile() {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... voids) {
                File cookiesFile = new File(mFileName);
                if (cookiesFile.exists()) {
                    if (!cookiesFile.delete()) Log.e(TAG, "Failed to delete " + mFileName);
                }
                return null;
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private static final class DestroyRunnable implements Runnable {
        private final long mNativeCookiesFetcher;

        private DestroyRunnable(long nativeCookiesFetcher) {
            mNativeCookiesFetcher = nativeCookiesFetcher;
        }

        @Override
        public void run() {
            if (mNativeCookiesFetcher != 0) nativeDestroy(mNativeCookiesFetcher);
        }
    }

    @CalledByNative
    private CanonicalCookie[] createCookiesArray(int size) {
        return new CanonicalCookie[size];
    }

    private native long nativeInit();
    private static native void nativeDestroy(long nativeCookiesFetcher);
    private native void nativeFetchFromCookieJar(long nativeCookiesFetcher);
    private native void nativeRestoreToCookieJar(long nativeCookiesFetcher,
            String url, String name, String value, String domain, String path,
            long creation, long expiration, long lastAccess, boolean secure,
            boolean httpOnly, int priority);
}

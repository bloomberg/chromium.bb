// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;
import android.webkit.ValueCallback;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.X509EncodedKeySpec;

import javax.crypto.Cipher;
import javax.crypto.EncryptedPrivateKeyInfo;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;

/**
 * AwTokenBindingManager manages the token binding protocol.
 *
 * see https://tools.ietf.org/html/draft-ietf-tokbind-protocol-03
 *
 * The token binding protocol can be enabled for browser context
 * separately. However all webviews share the same browser context and do not
 * expose the browser context to the embedder app. As such, there is no way to
 * enable token binding manager for individual webviews.
 */
@JNINamespace("android_webview")
public final class AwTokenBindingManager {
    private static final String TAG = "TokenBindingManager";
    private static final String PASSWORD = "";
    private static final String ALGORITHM = "PBEWITHSHAAND3-KEYTRIPLEDES-CBC";
    private static final String ELLIPTIC_CURVE = "EC";

    public void enableTokenBinding() {
        nativeEnableTokenBinding();
    }

    public void getKey(Uri origin, String[] spec, ValueCallback<KeyPair> callback) {
        if (callback == null) {
            throw new IllegalArgumentException("callback can't be null");
        }
        nativeGetTokenBindingKey(origin.getHost(), callback);
    }

    public void deleteKey(Uri origin, ValueCallback<Boolean> callback) {
        if (origin == null) {
            throw new IllegalArgumentException("origin can't be null");
        }
        // null callback is allowed
        nativeDeleteTokenBindingKey(origin.getHost(), callback);
    }

    public void deleteAllKeys(ValueCallback<Boolean> callback) {
        // null callback is allowed
        nativeDeleteAllTokenBindingKeys(callback);
    }

    @CalledByNative
    private static void onKeyReady(
            ValueCallback<KeyPair> callback, byte[] privateKeyBytes, byte[] publicKeyBytes) {
        if (privateKeyBytes == null || publicKeyBytes == null) {
            callback.onReceiveValue(null);
            return;
        }
        KeyPair keyPair = null;
        try {
            EncryptedPrivateKeyInfo epkInfo = new EncryptedPrivateKeyInfo(privateKeyBytes);
            SecretKeyFactory secretKeyFactory = SecretKeyFactory.getInstance(ALGORITHM);
            Key key = secretKeyFactory.generateSecret(new PBEKeySpec(PASSWORD.toCharArray()));
            Cipher cipher = Cipher.getInstance(ALGORITHM);
            cipher.init(Cipher.DECRYPT_MODE, key, epkInfo.getAlgParameters());
            KeyFactory factory = KeyFactory.getInstance(ELLIPTIC_CURVE);
            PrivateKey privateKey = factory.generatePrivate(epkInfo.getKeySpec(cipher));
            PublicKey publicKey =
                    factory.generatePublic(new X509EncodedKeySpec(publicKeyBytes));
            keyPair = new KeyPair(publicKey, privateKey);
        } catch (NoSuchAlgorithmException | InvalidKeySpecException | IOException
                | NoSuchPaddingException | InvalidKeyException
                | InvalidAlgorithmParameterException ex) {
            Log.e(TAG, "Failed converting key ", ex);
        }
        callback.onReceiveValue(keyPair);
    }

    @CalledByNative
    private static void onDeletionComplete(ValueCallback<Boolean> callback) {
        // At present, the native deletion complete callback always succeeds.
        callback.onReceiveValue(true);
    }

    private native void nativeEnableTokenBinding();
    private native void nativeGetTokenBindingKey(String host, ValueCallback<KeyPair> callback);
    private native void nativeDeleteTokenBindingKey(String host, ValueCallback<Boolean> callback);
    private native void nativeDeleteAllTokenBindingKeys(ValueCallback<Boolean> callback);
}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Log;

import java.lang.reflect.Method;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.Signature;
import java.security.interfaces.DSAKey;
import java.security.interfaces.DSAParams;
import java.security.interfaces.DSAPrivateKey;
import java.security.interfaces.ECKey;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.RSAKey;
import java.security.interfaces.RSAPrivateKey;
import java.security.spec.ECParameterSpec;

/**
 * Simple implementation of the AndroidKeyStore for use with an in-process Java KeyStore.
 */
public class DefaultAndroidKeyStore implements AndroidKeyStore {

    private static final String TAG = "AndroidKeyStoreInProcessImpl";

    private static class DefaultAndroidPrivateKey implements AndroidPrivateKey {
        // The actual Java key being wrapped.
        final PrivateKey mKey;
        // Key store handling this key.
        final DefaultAndroidKeyStore mStore;

        DefaultAndroidPrivateKey(PrivateKey key, DefaultAndroidKeyStore store) {
            mKey = key;
            mStore = store;
        }

        PrivateKey getJavaKey() {
            return mKey;
        }

        @Override
        public AndroidKeyStore getKeyStore() {
            return mStore;
        }
    }

    public AndroidPrivateKey createKey(PrivateKey javaKey) {
        return new DefaultAndroidPrivateKey(javaKey, this);
    }

    @Override
    public byte[] getRSAKeyModulus(AndroidPrivateKey key) {
        PrivateKey javaKey = ((DefaultAndroidPrivateKey) key).getJavaKey();
        if (javaKey instanceof RSAKey) {
            return ((RSAKey) javaKey).getModulus().toByteArray();
        }
        Log.w(TAG, "Not a RSAKey instance!");
        return null;
    }

    @Override
    public byte[] getDSAKeyParamQ(AndroidPrivateKey key) {
        PrivateKey javaKey = ((DefaultAndroidPrivateKey) key).getJavaKey();
        if (javaKey instanceof DSAKey) {
            DSAParams params = ((DSAKey) javaKey).getParams();
            return params.getQ().toByteArray();
        }
        Log.w(TAG, "Not a DSAKey instance!");
        return null;
    }

    @Override
    public byte[] getECKeyOrder(AndroidPrivateKey key) {
        PrivateKey javaKey = ((DefaultAndroidPrivateKey) key).getJavaKey();
        if (javaKey instanceof ECKey) {
            ECParameterSpec params = ((ECKey) javaKey).getParams();
            return params.getOrder().toByteArray();
        }
        Log.w(TAG, "Not an ECKey instance!");
        return null;
    }

   @Override
    public byte[] getPrivateKeyEncodedBytes(AndroidPrivateKey key) {
        PrivateKey javaKey = ((DefaultAndroidPrivateKey) key).getJavaKey();
        return javaKey.getEncoded();
    }

    @Override
    public byte[] rawSignDigestWithPrivateKey(AndroidPrivateKey key,
                                                     byte[] message) {
        PrivateKey javaKey = ((DefaultAndroidPrivateKey) key).getJavaKey();
        // Get the Signature for this key.
        Signature signature = null;
        // Hint: Algorithm names come from:
        // http://docs.oracle.com/javase/6/docs/technotes/guides/security/StandardNames.html
        try {
            if (javaKey instanceof RSAPrivateKey) {
                // IMPORTANT: Due to a platform bug, this will throw NoSuchAlgorithmException
                // on Android 4.0.x and 4.1.x. Fixed in 4.2 and higher.
                // See https://android-review.googlesource.com/#/c/40352/
                signature = Signature.getInstance("NONEwithRSA");
            } else if (javaKey instanceof DSAPrivateKey) {
                signature = Signature.getInstance("NONEwithDSA");
            } else if (javaKey instanceof ECPrivateKey) {
                signature = Signature.getInstance("NONEwithECDSA");
            }
        } catch (NoSuchAlgorithmException e) {
            ;
        }

        if (signature == null) {
            Log.e(TAG, "Unsupported private key algorithm: " + javaKey.getAlgorithm());
            return null;
        }

        // Sign the message.
        try {
            signature.initSign(javaKey);
            signature.update(message);
            return signature.sign();
        } catch (Exception e) {
            Log.e(TAG, "Exception while signing message with " + javaKey.getAlgorithm() +
                        " private key: " + e);
            return null;
        }
    }

    @Override
    public int getPrivateKeyType(AndroidPrivateKey key) {
        PrivateKey javaKey = ((DefaultAndroidPrivateKey) key).getJavaKey();
        if (javaKey instanceof RSAPrivateKey)
            return PrivateKeyType.RSA;
        if (javaKey instanceof DSAPrivateKey)
            return PrivateKeyType.DSA;
        if (javaKey instanceof ECPrivateKey)
            return PrivateKeyType.ECDSA;
        else
            return PrivateKeyType.INVALID;
    }

    private Object getOpenSSLKeyForPrivateKey(AndroidPrivateKey key) {
        PrivateKey javaKey = ((DefaultAndroidPrivateKey) key).getJavaKey();
        // Sanity checks
        if (javaKey == null) {
            Log.e(TAG, "key == null");
            return null;
        }
        if (!(javaKey instanceof RSAPrivateKey)) {
            Log.e(TAG, "does not implement RSAPrivateKey");
            return null;
        }
        // First, check that this is a proper instance of OpenSSLRSAPrivateKey
        // or one of its sub-classes.
        Class<?> superClass;
        try {
            superClass = Class.forName(
                    "org.apache.harmony.xnet.provider.jsse.OpenSSLRSAPrivateKey");
        } catch (Exception e) {
            // This may happen if the target device has a completely different
            // implementation of the java.security APIs, compared to vanilla
            // Android. Highly unlikely, but still possible.
            Log.e(TAG, "Cannot find system OpenSSLRSAPrivateKey class: " + e);
            return null;
        }
        if (!superClass.isInstance(javaKey)) {
            // This may happen if the PrivateKey was not created by the "AndroidOpenSSL"
            // provider, which should be the default. That could happen if an OEM decided
            // to implement a different default provider. Also highly unlikely.
            Log.e(TAG, "Private key is not an OpenSSLRSAPrivateKey instance, its class name is:" +
                       javaKey.getClass().getCanonicalName());
            return null;
        }

        try {
            // Use reflection to invoke the 'getOpenSSLKey()' method on the
            // private key. This returns another Java object that wraps a native
            // EVP_PKEY and OpenSSLEngine. Note that the method is final in Android
            // 4.1, so calling the superclass implementation is ok.
            Method getKey = superClass.getDeclaredMethod("getOpenSSLKey");
            getKey.setAccessible(true);
            Object opensslKey = null;
            try {
                opensslKey = getKey.invoke(javaKey);
            } finally {
                getKey.setAccessible(false);
            }
            if (opensslKey == null) {
                // Bail when detecting OEM "enhancement".
                Log.e(TAG, "getOpenSSLKey() returned null");
                return null;
            }
            return opensslKey;
        } catch (Exception e) {
            Log.e(TAG, "Exception while trying to retrieve system EVP_PKEY handle: " + e);
            return null;
        }
    }

    @Override
    public long getOpenSSLHandleForPrivateKey(AndroidPrivateKey key) {
        Object opensslKey = getOpenSSLKeyForPrivateKey(key);
        if (opensslKey == null)
            return 0;

        try {
            // Use reflection to invoke the 'getPkeyContext' method on the
            // result of the getOpenSSLKey(). This is an 32-bit integer
            // which is the address of an EVP_PKEY object. Note that this
            // method these days returns a 64-bit long, but since this code
            // path is used for older Android versions, it may still return
            // a 32-bit int here. To be on the safe side, we cast the return
            // value via Number rather than directly to Integer or Long.
            Method getPkeyContext;
            try {
                getPkeyContext = opensslKey.getClass().getDeclaredMethod("getPkeyContext");
            } catch (Exception e) {
                // Bail here too, something really not working as expected.
                Log.e(TAG, "No getPkeyContext() method on OpenSSLKey member:" + e);
                return 0;
            }
            getPkeyContext.setAccessible(true);
            long evp_pkey = 0;
            try {
                evp_pkey = ((Number) getPkeyContext.invoke(opensslKey)).longValue();
            } finally {
                getPkeyContext.setAccessible(false);
            }
            if (evp_pkey == 0) {
                // The PrivateKey is probably rotten for some reason.
                Log.e(TAG, "getPkeyContext() returned null");
            }
            return evp_pkey;

        } catch (Exception e) {
            Log.e(TAG, "Exception while trying to retrieve system EVP_PKEY handle: " + e);
            return 0;
        }
    }

    @Override
    public Object getOpenSSLEngineForPrivateKey(AndroidPrivateKey key) {
        // Find the system OpenSSLEngine class.
        Class<?> engineClass;
        try {
            engineClass = Class.forName(
                    "org.apache.harmony.xnet.provider.jsse.OpenSSLEngine");
        } catch (Exception e) {
            // This may happen if the target device has a completely different
            // implementation of the java.security APIs, compared to vanilla
            // Android. Highly unlikely, but still possible.
            Log.e(TAG, "Cannot find system OpenSSLEngine class: " + e);
            return null;
        }

        Object opensslKey = getOpenSSLKeyForPrivateKey(key);
        if (opensslKey == null)
            return null;

        try {
            // Use reflection to invoke the 'getEngine' method on the
            // result of the getOpenSSLKey().
            Method getEngine;
            try {
                getEngine = opensslKey.getClass().getDeclaredMethod("getEngine");
            } catch (Exception e) {
                // Bail here too, something really not working as expected.
                Log.e(TAG, "No getEngine() method on OpenSSLKey member:" + e);
                return null;
            }
            getEngine.setAccessible(true);
            Object engine = null;
            try {
                engine = getEngine.invoke(opensslKey);
            } finally {
                getEngine.setAccessible(false);
            }
            if (engine == null) {
                // The PrivateKey is probably rotten for some reason.
                Log.e(TAG, "getEngine() returned null");
            }
            // Sanity-check the returned engine.
            if (!engineClass.isInstance(engine)) {
                Log.e(TAG, "Engine is not an OpenSSLEngine instance, its class name is:" +
                        engine.getClass().getCanonicalName());
                return null;
            }
            return engine;

        } catch (Exception e) {
            Log.e(TAG, "Exception while trying to retrieve OpenSSLEngine object: " + e);
            return null;
        }
    }

    @Override
    public void releaseKey(AndroidPrivateKey key) {
        // no-op for in-process. GC will handle key collection
    }
}

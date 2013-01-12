// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.SharedPreferences;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * An implementation of SharedPreferences that can be used in tests.
 * <p/>
 * It keeps all state in memory, and there is no difference between apply() and commit().
 */
public class InMemorySharedPreferences implements SharedPreferences {

    private final Map<String, Object> mData;

    private final SharedPreferences.Editor mEditor = new InMemoryEditor();

    public InMemorySharedPreferences() {
        mData = new HashMap<String, Object>();
    }

    public InMemorySharedPreferences(Map<String, Object> data) {
        mData = data;
    }

    @Override
    public Map<String, ?> getAll() {
        return mData;
    }

    @Override
    public String getString(String key, String defValue) {
        if (mData.containsKey(key)) {
            return (String) mData.get(key);
        }
        return defValue;
    }

    @SuppressWarnings("unchecked")
    @Override
    public Set<String> getStringSet(String key, Set<String> defValues) {
        if (mData.containsKey(key)) {
            return (Set<String>) mData.get(key);
        }
        return defValues;
    }

    @Override
    public int getInt(String key, int defValue) {
        if (mData.containsKey(key)) {
            return (Integer) mData.get(key);
        }
        return defValue;
    }

    @Override
    public long getLong(String key, long defValue) {
        if (mData.containsKey(key)) {
            return (Long) mData.get(key);
        }
        return defValue;
    }

    @Override
    public float getFloat(String key, float defValue) {
        if (mData.containsKey(key)) {
            return (Float) mData.get(key);
        }
        return defValue;
    }

    @Override
    public boolean getBoolean(String key, boolean defValue) {
        if (mData.containsKey(key)) {
            return (Boolean) mData.get(key);
        }
        return defValue;
    }

    @Override
    public boolean contains(String key) {
        return mData.containsKey(key);
    }

    @Override
    public SharedPreferences.Editor edit() {
        return mEditor;
    }

    @Override
    public void registerOnSharedPreferenceChangeListener(
            SharedPreferences.OnSharedPreferenceChangeListener
                    listener) {
    }

    @Override
    public void unregisterOnSharedPreferenceChangeListener(
            SharedPreferences.OnSharedPreferenceChangeListener listener) {
    }

    private class InMemoryEditor implements SharedPreferences.Editor {

        private boolean mClearCalled;

        private final Map<String, Object> mChanges = new HashMap<String, Object>();

        @Override
        public SharedPreferences.Editor putString(String key, String value) {
            mChanges.put(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor putStringSet(String key, Set<String> values) {
            mChanges.put(key, values);
            return this;
        }

        @Override
        public SharedPreferences.Editor putInt(String key, int value) {
            mChanges.put(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor putLong(String key, long value) {
            mChanges.put(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor putFloat(String key, float value) {
            mChanges.put(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor putBoolean(String key, boolean value) {
            mChanges.put(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor remove(String key) {
            // Magic value for removes
            mChanges.put(key, this);
            return this;
        }

        @Override
        public SharedPreferences.Editor clear() {
            mClearCalled = true;
            return this;
        }

        @Override
        public boolean commit() {
            apply();
            return true;
        }

        @Override
        public void apply() {
            if (mClearCalled) {
                mData.clear();
            }
            for (Map.Entry<String, Object> entry : mChanges.entrySet()) {
                String key = entry.getKey();
                Object value = entry.getValue();
                if (value == this) {
                    // Special value for removal
                    mData.remove(key);
                } else {
                    mData.put(key, value);
                }
            }
        }

    }

}

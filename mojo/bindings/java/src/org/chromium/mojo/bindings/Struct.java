// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Core;

/**
 * Base class for all mojo structs.
 */
public abstract class Struct {

    /**
     * The header for a mojo complex element.
     */
    public static final class DataHeader {

        /**
         * The size of a serialized header, in bytes.
         */
        public static final int HEADER_SIZE = 8;

        /**
         * The offset of the size field.
         */
        public static final int SIZE_OFFSET = 0;

        /**
         * The offset of the number of fields field.
         */
        public static final int NUM_FIELDS_OFFSET = 4;

        public final int size;
        public final int numFields;

        /**
         * Constructor.
         */
        public DataHeader(int size, int numFields) {
            super();
            this.size = size;
            this.numFields = numFields;
        }
    }

    /**
     * The base size of the struct.
     */
    protected final int mEncodedBaseSize;

    /**
     * Constructor.
     */
    protected Struct(int encodedBaseSize) {
        this.mEncodedBaseSize = encodedBaseSize;
    }

    /**
     * Use the given encoder to serialized this struct.
     */
    protected abstract void encode(Encoder encoder);

    /**
     * Returns the serialization of the struct. This method can close Handles.
     *
     * @param core the |Core| implementation used to generate handles. Only used if the |Struct|
     *            being encoded contains interfaces, can be |null| otherwise.
     */
    public Message serialize(Core core) {
        Encoder encoder = new Encoder(core, mEncodedBaseSize);
        encode(encoder);
        return encoder.getMessage();
    }

}

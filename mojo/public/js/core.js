// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "mojo/public/js/core"
//
// Note: This file is for documentation purposes only. The code here is not
// actually executed. The real module is implemented natively in Mojo.
//
// This module provides the JavaScript bindings for mojo/public/c/system/core.h.
// Refer to that file for more detailed documentation for equivalent methods.

while (1);

/**
 * MojoHandle: An opaque handles to a Mojo object (e.g. a message pipe).
 */
var kInvalidHandle;

/**
 * MojoResult {number}: Result codes for Mojo operations.
 * See core.h for more information.
 */
var RESULT_OK;
var RESULT_CANCELLED;
var RESULT_UNKNOWN;
var RESULT_INVALID_ARGUMENT;
var RESULT_DEADLINE_EXCEEDED;
var RESULT_NOT_FOUND;
var RESULT_ALREADY_EXISTS;
var RESULT_PERMISSION_DENIED;
var RESULT_RESOURCE_EXHAUSTED;
var RESULT_FAILED_PRECONDITION;
var RESULT_ABORTED;
var RESULT_OUT_OF_RANGE;
var RESULT_UNIMPLEMENTED;
var RESULT_INTERNAL;
var RESULT_UNAVAILABLE;
var RESULT_DATA_LOSS;
var RESULT_BUSY;
var RESULT_SHOULD_WAIT;

/**
 * MojoHandleSignals: Used to specify signals that can be waited on for a handle
 *(and which can be triggered), e.g., the ability to read or write to
 * the handle.
 * See core.h for more information.
 */
var HANDLE_SIGNAL_NONE;
var HANDLE_SIGNAL_READABLE;
var HANDLE_SIGNAL_WRITABLE;
var HANDLE_SIGNAL_PEER_CLOSED;
var HANDLE_SIGNAL_PEER_REMOTE;

/**
 * MojoCreateDataMessageOptions: Used to specify creation parameters for a data
 * pipe to |createDataMessage()|.
 * See core.h for more information.
 */
dictionary MojoCreateDataMessageOptions {
  MojoCreateDataMessageOptionsFlags flags;  // See below.
};

// MojoCreateDataMessageOptionsFlags
var CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE;

/*
 * MojoWriteMessageFlags: Used to specify different modes to |writeMessage()|.
 * See core.h for more information.
 */
var WRITE_MESSAGE_FLAG_NONE;

/**
 * MojoReadMessageFlags: Used to specify different modes to |readMessage()|.
 * See core.h for more information.
 */
var READ_MESSAGE_FLAG_NONE;

/**
 * MojoCreateDataPipeOptions: Used to specify creation parameters for a data
 * pipe to |createDataPipe()|.
 * See core.h for more information.
 */
dictionary MojoCreateDataPipeOptions {
  MojoCreateDataPipeOptionsFlags flags;  // See below.
  int32 elementNumBytes;  // The size of an element, in bytes.
  int32 capacityNumBytes;  // The capacity of the data pipe, in bytes.
};

// MojoCreateDataPipeOptionsFlags
var CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;

/*
 * MojoWriteDataFlags: Used to specify different modes to |writeData()|.
 * See core.h for more information.
 */
var WRITE_DATA_FLAG_NONE;
var WRITE_DATA_FLAG_ALL_OR_NONE;

/**
 * MojoReadDataFlags: Used to specify different modes to |readData()|.
 * See core.h for more information.
 */
var READ_DATA_FLAG_NONE;
var READ_DATA_FLAG_ALL_OR_NONE;
var READ_DATA_FLAG_DISCARD;
var READ_DATA_FLAG_QUERY;
var READ_DATA_FLAG_PEEK;

/**
 * MojoCreateSharedBufferOptionsFlags: Used to specify options to
 *   |createSharedBuffer()|.
 * See core.h for more information.
 */
var CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE;

/**
 * MojoDuplicateBufferHandleOptionsFlags: Used to specify options to
 *   |duplicateBufferHandle()|.
 * See core.h for more information.
 */
var DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE;
var DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_READ_ONLY;

/**
 * MojoMapBufferFlags: Used to specify options to |mapBuffer()|.
 * See core.h for more information.
 */
var MAP_BUFFER_FLAG_NONE;

/**
 * Closes the given |handle|. See MojoClose for more info.
 * @param {MojoHandle} Handle to close.
 * @return {MojoResult} Result code.
 */
function close(handle) { [native code] }

/**
 * Queries the last known signaling state of |handle|.
 *
 * @param {MojoHandle} handle Handle to query.
 * @return {object} An object of the form {
 *     result,              // MOJO_RESULT_OK or MOJO_RESULT_INVALID_ARGUMENT
 *     satisfiedSignals,    // MojoHandleSignals (see above)
 *     satisfiableSignals,  // MojoHandleSignals
 * }
 */
function queryHandleSignalsState(handle) { [native code] }

/**
 * Waits on the given handle until a signal indicated by |signals| is
 * satisfied or an error occurs.
 *
 * @param {MojoHandle} handle Handle to wait on.
 * @param {MojoHandleSignals} signals Specifies the condition to wait for.
 * @return {MojoResult} Result code.
 */
function wait(handle, signals) { [native code] }

/**
 * Creates a message pipe. This function always succeeds.
 * See MojoCreateMessagePipe for more information on message pipes.
 *
 * @param {MojoCreateMessagePipeOptions} optionsDict Options to control the
 * message pipe parameters. May be null.
 * @return {MessagePipe} An object of the form {
 *     handle0,
 *     handle1,
 *   }
 *   where |handle0| and |handle1| are MojoHandles to each end of the channel.
 */
function createMessagePipe(optionsDict) { [native code] }

/**
 * Writes a message to the message pipe endpoint given by |handle|. See
 * MojoWriteMessage for more information, including return codes.
 *
 * @param {MojoHandle} handle The endpoint to write to.
 * @param {ArrayBufferView} buffer The message data. May be empty.
 * @param {Array.MojoHandle} handlesArray Any handles to attach. Handles are
 *   transferred on success and will no longer be valid. May be empty.
 * @param {MojoWriteMessageFlags} flags Flags.
 * @return {MojoResult} Result code.
 */
function writeMessage(handle, buffer, handlesArray, flags) { [native code] }

/**
 * Reads a message from the message pipe endpoint given by |handle|. See
 * MojoReadMessage for more information, including return codes.
 *
 * @param {MojoHandle} handle The endpoint to read from.
 * @param {MojoReadMessageFlags} flags Flags.
 * @return {object} An object of the form {
 *     result,  // |RESULT_OK| on success, error code otherwise.
 *     buffer,  // An ArrayBufferView of the message data (only on success).
 *     handles  // An array of MojoHandles transferred, if any.
 *   }
 */
function readMessage(handle, flags) { [native code] }

/**
 * Creates a data pipe, which is a unidirectional communication channel for
 * unframed data, with the given options. See MojoCreateDataPipe for more
 * more information, including return codes.
 *
 * @param {MojoCreateDataPipeOptions} optionsDict Options to control the data
 *   pipe parameters. May be null.
 * @return {object} An object of the form {
 *     result,  // |RESULT_OK| on success, error code otherwise.
 *     producerHandle,  // MojoHandle to use with writeData (only on success).
 *     consumerHandle,  // MojoHandle to use with readData (only on success).
 *   }
 */
function createDataPipe(optionsDict) { [native code] }

/**
 * Writes the given data to the data pipe producer given by |handle|. See
 * MojoWriteData for more information, including return codes.
 *
 * @param {MojoHandle} handle A producerHandle returned by createDataPipe.
 * @param {ArrayBufferView} buffer The data to write.
 * @param {MojoWriteDataFlags} flags Flags.
 * @return {object} An object of the form {
 *     result,  // |RESULT_OK| on success, error code otherwise.
 *     numBytes,  // The number of bytes written.
 *   }
 */
function writeData(handle, buffer, flags) { [native code] }

/**
 * Reads data from the data pipe consumer given by |handle|. May also
 * be used to discard data. See MojoReadData for more information, including
 * return codes.
 *
 * @param {MojoHandle} handle A consumerHandle returned by createDataPipe.
 * @param {MojoReadDataFlags} flags Flags.
 * @return {object} An object of the form {
 *     result,  // |RESULT_OK| on success, error code otherwise.
 *     buffer,  // An ArrayBufferView of the data read (only on success).
 *   }
 */
function readData(handle, flags) { [native code] }

/**
 * True if the argument is a message or data pipe handle.
 *
 * @param {value} an arbitrary JS value.
 * @return true or false
 */
function isHandle(value) { [native code] }

/**
 * Creates shared buffer of specified size |num_bytes|.
 * See MojoCreateSharedBuffer for more information including error codes.
 *
 * @param {number} num_bytes Size of the memory to be allocated for shared
 * @param {MojoCreateSharedBufferOptionsFlags} flags Flags.
 *   buffer.
 * @return {object} An object of the form {
 *     result,  // |RESULT_OK| on success, error code otherwise.
 *     handle,  // An MojoHandle for shared buffer (only on success).
 *   }
 */
function createSharedBuffer(num_bytes, flags) { [native code] }

/**
 * Duplicates the |buffer_handle| to a shared buffer. Duplicated handle can be
 * sent to another process over message pipe. See MojoDuplicateBufferHandle for
 * more information including error codes.
 *
 * @param {MojoHandle} buffer_handle MojoHandle.
 * @param {MojoCreateSharedBufferOptionsFlags} flags Flags.
 * @return {object} An object of the form {
 *     result,  // |RESULT_OK| on success, error code otherwise.
 *     handle,  // A duplicated MojoHandle for shared buffer (only on success).
 *   }
 */
function duplicateBufferHandle(buffer_handle, flags) { [native code] }

/**
 * Maps the part (at offset |offset| of length |num_bytes|) of the buffer given
 * by |buffer_handle| into ArrayBuffer memory |buffer|, with options specified
 * by |flags|. See MojoMapBuffer for more information including error codes.
 *
 * @param {MojoHandle} buffer_handle A sharedBufferHandle returned by
 *   createSharedBuffer.
 * @param {number} offset Offset.
 * @param {number} num_bytes Size of the memory to be mapped.
 * @param {MojoMapBufferFlags} flags Flags.
 * @return {object} An object of the form {
 *     result,  // |RESULT_OK| on success, error code otherwise.
 *     buffer,  // An ArrayBuffer (only on success).
 *   }
 */
function mapBuffer(buffer_handle, offset, num_bytes, flags) { [native code] }

/**
 * Unmaps buffer that was mapped using mapBuffer.
 * See MojoUnmapBuffer for more information including error codes.
 *
 * @param {ArrayBuffer} buffer ArrayBuffer.
 * @return {MojoResult} Result code.
 */
function unmapBuffer(buffer) { [native code] }

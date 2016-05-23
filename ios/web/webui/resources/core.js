// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "mojo/public/js/core"
//
// This module provides the JavaScript bindings for mojo/public/c/system/core.h.
// Refer to that file for more detailed documentation for equivalent methods.

define("mojo/public/js/core", [
  "ios/mojo/public/js/sync_message_channel",
  "ios/mojo/public/js/handle_util",
], function(syncMessageChannel, handleUtil) {

  /**
   * Closes the given |handle|. See MojoClose for more info.
   * @param {!MojoHandle} handle Handle to close.
   * @return {!MojoResult} Result code.
   */
  function close(handle) {
    handleUtil.invalidateHandle(handle);
    return syncMessageChannel.sendMessage({
      name: "core.close",
      args: {
        handle: handleUtil.getNativeHandle(handle)
      }
    });
  }

  /**
   * Waits on the given handle until a signal indicated by |signals| is
   * satisfied or until |deadline| is passed. See MojoWait for more information.
   *
   * @param {!MojoHandle} handle Handle to wait on.
   * @param {!MojoHandleSignals} signals Specifies the condition to wait for.
   * @param {!MojoDeadline} deadline Stops waiting if this is reached.
   * @return {!MojoResult} Result code.
   */
  function wait(handle, signals, deadline) {
    console.error('wait is not implemented');
  }

  /**
   * Waits on |handles[0]|, ..., |handles[handles.length-1]| for at least one of
   * them to satisfy the state indicated by |flags[0]|, ...,
   * |flags[handles.length-1]|, respectively, or until |deadline| has passed.
   * See MojoWaitMany for more information.
   *
   * @param {!Array.MojoHandle} handles Handles to wait on.
   * @param {!Array.MojoHandleSignals} signals Specifies the condition to wait
   *        for, for each corresponding handle. Must be the same length as
   *        |handles|.
   * @param {!MojoDeadline} deadline Stops waiting if this is reached.
   * @return {!MojoResult} Result code.
   */
  function waitMany(handles, signals, deadline) {
    console.error('wait is not implemented');
  }

  /**
   * Creates a message pipe. This function always succeeds.
   * See MojoCreateMessagePipe for more information on message pipes.
   *
   * @param {!MojoCreateMessagePipeOptions} optionsDict Options to control the
   * message pipe parameters. May be null.
   * @return {!MessagePipe} An object of the form {
   *     handle0,
   *     handle1,
   *   }
   *   where |handle0| and |handle1| are MojoHandles to each end of the channel.
   */
  function createMessagePipe(optionsDict) {
    var result = syncMessageChannel.sendMessage({
      name: "core.createMessagePipe",
      args: {
        optionsDict: optionsDict || null
      }
    });
    result.handle0 = handleUtil.getJavaScriptHandle(result.handle0);
    result.handle1 = handleUtil.getJavaScriptHandle(result.handle1);
    return result;
  }

  /**
   * Writes a message to the message pipe endpoint given by |handle|. See
   * MojoWriteMessage for more information, including return codes.
   *
   * @param {!MojoHandle} handle The endpoint to write to.
   * @param {!ArrayBufferView} buffer The message data. May be empty.
   * @param {!Array.MojoHandle} handles Any handles to attach. Handles are
   *   transferred on success and will no longer be valid. May be empty.
   * @param {!MojoWriteMessageFlags} flags Flags.
   * @return {!MojoResult} Result code.
   */
  function writeMessage(handle, buffer, handles, flags) {
    return syncMessageChannel.sendMessage({
      name: "core.writeMessage",
      args: {
        handle: handleUtil.getNativeHandle(handle),
        buffer: buffer,
        handles: handleUtil.getNativeHandles(handles),
        flags: flags || null
      }
    });
  }

  /**
   * Reads a message from the message pipe endpoint given by |handle|. See
   * MojoReadMessage for more information, including return codes.
   *
   * @param {!MojoHandle} handle The endpoint to read from.
   * @param {!MojoReadMessageFlags} flags Flags.
   * @return {!Object} An object of the form {
   *     result,  // |RESULT_OK| on success, error code otherwise.
   *     buffer,  // An ArrayBufferView of the message data (only on success).
   *     handles  // An array of MojoHandles transferred, if any.
   *   }
   */
  function readMessage(handle, flags) {
    var result = syncMessageChannel.sendMessage({
      name: "core.readMessage",
      args: {
        handle: handleUtil.getNativeHandle(handle),
        flags: flags
      }
    });
    result.buffer = new Uint8Array(result.buffer || []).buffer;
    return result;
  }

  /**
   * Creates a data pipe, which is a unidirectional communication channel for
   * unframed data, with the given options. See MojoCreateDataPipe for more
   * more information, including return codes.
   *
   * @param {!MojoCreateDataPipeOptions} optionsDict Options to control the data
   *   pipe parameters. May be null.
   * @return {!Object} An object of the form {
   *     result,  // |RESULT_OK| on success, error code otherwise.
   *     producerHandle,  // MojoHandle to use with writeData (only on success).
   *     consumerHandle,  // MojoHandle to use with readData (only on success).
   *   }
   */
  function createDataPipe(optionsDict) {
    console.error('createDataPipe is not implemented');
  }

  /**
   * Writes the given data to the data pipe producer given by |handle|. See
   * MojoWriteData for more information, including return codes.
   *
   * @param {!MojoHandle} handle A producerHandle returned by createDataPipe.
   * @param {!ArrayBufferView} buffer The data to write.
   * @param {!MojoWriteDataFlags} flags Flags.
   * @return {!Object} An object of the form {
   *     result,  // |RESULT_OK| on success, error code otherwise.
   *     numBytes,  // The number of bytes written.
   *   }
   */
  function writeData(handle, buffer, flags) {
    console.error('writeData is not implemented');
  }

  /**
   * Reads data from the data pipe consumer given by |handle|. May also
   * be used to discard data. See MojoReadData for more information, including
   * return codes.
   *
   * @param {!MojoHandle} handle A consumerHandle returned by createDataPipe.
   * @param {!MojoReadDataFlags} flags Flags.
   * @return {!Object} An object of the form {
   *     result,  // |RESULT_OK| on success, error code otherwise.
   *     buffer,  // An ArrayBufferView of the data read (only on success).
   *   }
   */
  function readData(handle, flags) {
    console.error('readData is not implemented');
  }

  /**
   * True if the argument is a message or data pipe handle.
   *
   * @param {*} value An arbitrary JS value.
   * @return {boolean}
   */
  function isHandle(value) {
    return handleUtil.isValidHandle(value);
  }

  var exports = {};
  exports.close = close;
  exports.wait = wait;
  exports.waitMany = waitMany;
  exports.createMessagePipe = createMessagePipe;
  exports.writeMessage = writeMessage;
  exports.readMessage = readMessage;
  exports.createDataPipe = createDataPipe;
  exports.writeData = writeData;
  exports.readData = readData;
  exports.isHandle = isHandle;

  /**
   * MojoResult {number}: Result codes for Mojo operations.
   * See core.h for more information.
   */
  exports.RESULT_OK = 0
  exports.RESULT_CANCELLED = 1;
  exports.RESULT_UNKNOWN = 2;
  exports.RESULT_INVALID_ARGUMENT = 3;
  exports.RESULT_DEADLINE_EXCEEDED = 4;
  exports.RESULT_NOT_FOUND = 5;
  exports.RESULT_ALREADY_EXISTS = 6;
  exports.RESULT_PERMISSION_DENIED = 7;
  exports.RESULT_RESOURCE_EXHAUSTED = 8;
  exports.RESULT_FAILED_PRECONDITION = 9;
  exports.RESULT_ABORTED = 10;
  exports.RESULT_OUT_OF_RANGE = 11;
  exports.RESULT_UNIMPLEMENTED = 12;
  exports.RESULT_INTERNAL = 13;
  exports.RESULT_UNAVAILABLE = 14;
  exports.RESULT_DATA_LOSS = 15;
  exports.RESULT_BUSY = 16;
  exports.RESULT_SHOULD_WAIT = 17;

  /**
   * MojoDeadline {number}: Used to specify deadlines (timeouts), in
   * microseconds.
   * See core.h for more information.
   */
  exports.DEADLINE_INDEFINITE = -1;

  /**
   * MojoHandleSignals: Used to specify signals that can be waited on for a
   * handle (and which can be triggered), e.g., the ability to read or write to
   * the handle.
   * See core.h for more information.
   */
  exports.HANDLE_SIGNAL_NONE = 0;
  exports.HANDLE_SIGNAL_READABLE = 1;
  exports.HANDLE_SIGNAL_WRITABLE = 2;
  exports.HANDLE_SIGNAL_PEER_CLOSED = 4;

  // MojoCreateDataMessageOptionsFlags
  exports.CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE = 0;

  /*
   * MojoWriteMessageFlags: Used to specify different modes to |writeMessage()|.
   * See core.h for more information.
   */
  exports.WRITE_MESSAGE_FLAG_NONE = 0;

  /**
   * MojoReadMessageFlags: Used to specify different modes to |readMessage()|.
   * See core.h for more information.
   */
  exports.READ_MESSAGE_FLAG_NONE = 0;
  exports.READ_MESSAGE_FLAG_MAY_DISCARD = 1;

  // MojoCreateDataPipeOptionsFlags
  exports.CREATE_DATA_PIPE_OPTIONS_FLAG_NONE = 0;

  /*
   * MojoWriteDataFlags: Used to specify different modes to |writeData()|.
   * See core.h for more information.
   */
  exports.WRITE_DATA_FLAG_NONE = 0;
  exports.WRITE_DATA_FLAG_ALL_OR_NONE = 1;

  /**
   * MojoReadDataFlags: Used to specify different modes to |readData()|.
   * See core.h for more information.
   */
  exports.READ_DATA_FLAG_NONE = 0;
  exports.READ_DATA_FLAG_ALL_OR_NONE = 1;
  exports.READ_DATA_FLAG_DISCARD = 2;
  exports.READ_DATA_FLAG_QUERY = 4;
  exports.READ_DATA_FLAG_PEEK = 8;

  return exports;
});

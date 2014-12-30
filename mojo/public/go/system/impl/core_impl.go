// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package impl

//#include "mojo/public/platform/native/system_thunks.h"
//#include "mojo/public/c/system/main.h"
import "C"
import "unsafe"

var core *CoreImpl

func init() {
	core = &CoreImpl{}
}

// CoreImpl is an implementation of the Mojo system APIs.
type CoreImpl struct {
}

func GetCore() *CoreImpl {
	return core
}

func (c *CoreImpl) GetTimeTicksNow() MojoTimeTicks {
	return (MojoTimeTicks)(C.MojoGetTimeTicksNow())
}

func (c *CoreImpl) Close(handle MojoHandle) MojoResult {
	return (MojoResult)(C.MojoClose(handle.cType()))
}

func (c *CoreImpl) Wait(handle MojoHandle, signal MojoHandleSignals, deadline MojoDeadline) (MojoResult, *MojoHandleSignalsState) {
	var signal_states C.struct_MojoHandleSignalsState
	result := C.MojoNewWait(handle.cType(), signal.cType(), deadline.cType(), &signal_states)
	return MojoResult(result), &MojoHandleSignalsState{MojoHandleSignals(signal_states.satisfied_signals), MojoHandleSignals(signal_states.satisfiable_signals)}
}

func (c *CoreImpl) WaitMany(handles []MojoHandle, signals []MojoHandleSignals, deadline MojoDeadline) (result MojoResult, index *uint32, state []MojoHandleSignalsState) {
	// Set "-1" using the instructions from http://blog.golang.org/constants
	cindex := ^C.uint32_t(0)
	cstate := make([]C.struct_MojoHandleSignalsState, len(handles))

	result = (MojoResult)(C.MojoNewWaitMany(cArrayMojoHandle(handles), cArrayMojoHandleSignals(signals), (C.uint32_t)(len(handles)), deadline.cType(), &cindex, &cstate[0]))

	if uint32(cindex) < uint32(len(handles)) {
		temp := uint32(cindex)
		index = &temp
	}

	if result != MOJO_RESULT_INVALID_ARGUMENT && result != MOJO_RESULT_RESOURCE_EXHAUSTED {
		state = make([]MojoHandleSignalsState, len(cstate))
		for i, value := range cstate {
			state[i] = NewMojoHandleSignalsState(value)
		}
	}

	return
}

func (c *CoreImpl) CreateMessagePipe(opts *MessagePipeOptions) (MojoResult, MojoHandle, MojoHandle) {
	var handle0, handle1 C.MojoHandle
	result := C.MojoCreateMessagePipe(opts.cType(), &handle0, &handle1)
	return (MojoResult)(result), (MojoHandle)(handle0), (MojoHandle)(handle1)
}

func (c *CoreImpl) WriteMessage(handle MojoHandle, msg []byte, attached []MojoHandle, flags MojoWriteMessageFlags) MojoResult {
	return (MojoResult)(C.MojoWriteMessage(handle.cType(), cArrayBytes(msg), (C.uint32_t)(len(msg)), cArrayMojoHandle(attached), (C.uint32_t)(len(attached)), flags.cType()))
}

func (c *CoreImpl) ReadMessage(handle MojoHandle, flags MojoReadMessageFlags) (MojoResult, []byte, []MojoHandle, uint32, uint32) {
	var num_bytes, num_handles C.uint32_t
	if result := C.MojoReadMessage(handle.cType(), nil, &num_bytes, nil, &num_handles, flags.cType()); result != C.MOJO_RESULT_RESOURCE_EXHAUSTED {
		return (MojoResult)(result), nil, nil, 0, 0
	}
	msg := make([]byte, (uint32)(num_bytes))
	attached := make([]MojoHandle, (uint32)(num_handles))
	result := C.MojoReadMessage(handle.cType(), cArrayBytes(msg), &num_bytes, cArrayMojoHandle(attached), &num_handles, (C.MojoReadMessageFlags)(flags))
	return (MojoResult)(result), msg, attached, (uint32)(num_bytes), (uint32)(num_handles)
}

func (c *CoreImpl) CreateDataPipe(opts *DataPipeOptions) (MojoResult, MojoHandle, MojoHandle) {
	var producer, consumer C.MojoHandle
	result := C.MojoCreateDataPipe(opts.cType(), &producer, &consumer)
	return (MojoResult)(result), (MojoHandle)(producer), (MojoHandle)(consumer)
}

func (c *CoreImpl) WriteData(producer MojoHandle, data []byte, flags MojoWriteDataFlags) (MojoResult, uint32) {
	num_bytes := (C.uint32_t)(len(data))
	result := C.MojoWriteData(producer.cType(), cArrayBytes(data), &num_bytes, flags.cType())
	return (MojoResult)(result), (uint32)(num_bytes)
}

func (c *CoreImpl) ReadData(consumer MojoHandle, flags MojoReadDataFlags) (MojoResult, []byte) {
	var num_bytes C.uint32_t
	var result C.MojoResult
	if result = C.MojoReadData(consumer.cType(), nil, &num_bytes, C.MOJO_READ_DATA_FLAG_QUERY); result != C.MOJO_RESULT_OK {
		return (MojoResult)(result), nil
	}
	data := make([]byte, (uint32)(num_bytes))
	result = C.MojoReadData(consumer.cType(), cArrayBytes(data), &num_bytes, flags.cType())
	return (MojoResult)(result), data
}

func (c *CoreImpl) CreateSharedBuffer(opts *SharedBufferOptions, numBytes uint64) (MojoResult, MojoHandle) {
	var handle C.MojoHandle
	result := C.MojoCreateSharedBuffer(opts.cType(), (C.uint64_t)(numBytes), &handle)
	return (MojoResult)(result), (MojoHandle)(handle)
}

func (c *CoreImpl) DuplicateBufferHandle(handle MojoHandle, opts *DuplicateBufferHandleOptions) (MojoResult, MojoHandle) {
	var duplicate C.MojoHandle
	result := C.MojoDuplicateBufferHandle(handle.cType(), opts.cType(), &duplicate)
	return (MojoResult)(result), (MojoHandle)(duplicate)
}

func (c *CoreImpl) MapBuffer(handle MojoHandle, offset uint64, numBytes uint64, flags MojoMapBufferFlags) (MojoResult, unsafe.Pointer) {
	var bufPtr unsafe.Pointer
	result := C.MojoMapBuffer(handle.cType(), (C.uint64_t)(offset), (C.uint64_t)(numBytes), &bufPtr, flags.cType())
	if result != C.MOJO_RESULT_OK {
		return (MojoResult)(result), nil
	}
	return MOJO_RESULT_OK, bufPtr
}

func (c *CoreImpl) UnmapBuffer(buffer unsafe.Pointer) MojoResult {
	return (MojoResult)(C.MojoUnmapBuffer(buffer))
}

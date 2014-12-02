// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package impl

//#include "mojo/public/platform/native/system_thunks.h"
//#include "mojo/public/c/system/main.h"
import "C"
import (
	"math"
	"unsafe"
)

// Go equivalent definitions of the various system types defined in Mojo.
// mojo/public/c/system/types.h
// mojo/public/c/system/data_pipe.h
// mojo/public/c/system/message_pipe.h
//
type MojoTimeTicks int64
type MojoHandle uint32
type MojoResult int32
type MojoDeadline uint64
type MojoHandleSignals uint32
type MojoWriteMessageFlags uint32
type MojoReadMessageFlags uint32
type MojoWriteDataFlags uint32
type MojoReadDataFlags uint32
type MojoCreateDataPipeOptionsFlags uint32
type MojoCreateMessagePipeOptionsFlags uint32
type MojoCreateSharedBufferOptionsFlags uint32
type MojoDuplicateBufferHandleOptionsFlags uint32
type MojoMapBufferFlags uint32

const (
	MOJO_DEADLINE_INDEFINITE        MojoDeadline = math.MaxUint64
	MOJO_HANDLE_INVALID             MojoHandle   = 0
	MOJO_RESULT_OK                  MojoResult   = 0
	MOJO_RESULT_CANCELLED                        = -1
	MOJO_RESULT_UNKNOWN                          = -2
	MOJO_RESULT_INVALID_ARGUMENT                 = -3
	MOJO_RESULT_DEADLINE_EXCEEDED                = -4
	MOJO_RESULT_NOT_FOUND                        = -5
	MOJO_RESULT_ALREADY_EXISTS                   = -6
	MOJO_RESULT_PERMISSION_DENIED                = -7
	MOJO_RESULT_RESOURCE_EXHAUSTED               = -8
	MOJO_RESULT_FAILED_PRECONDITION              = -9
	MOJO_RESULT_ABORTED                          = -10
	MOJO_RESULT_OUT_OF_RANGE                     = -11
	MOJO_RESULT_UNIMPLEMENTED                    = -12
	MOJO_RESULT_INTERNAL                         = -13
	MOJO_RESULT_UNAVAILABLE                      = -14
	MOJO_RESULT_DATA_LOSS                        = -15
	MOJO_RESULT_BUSY                             = -16
	MOJO_RESULT_SHOULD_WAIT                      = -17

	MOJO_HANDLE_SIGNAL_NONE     MojoHandleSignals = 0
	MOJO_HANDLE_SIGNAL_READABLE                   = 1 << 0
	MOJO_HANDLE_SIGNAL_WRITABLE                   = 1 << 1
	MOJO_HANDLE_SIGNAL_PEER_CLOSED                = 1 << 2

	MOJO_WRITE_MESSAGE_FLAG_NONE       MojoWriteMessageFlags = 0
	MOJO_READ_MESSAGE_FLAG_NONE        MojoReadMessageFlags  = 0
	MOJO_READ_MESSAGE_FLAG_MAY_DISCARD                       = 1 << 0

	MOJO_READ_DATA_FLAG_NONE         MojoReadDataFlags  = 0
	MOJO_READ_DATA_FLAG_ALL_OR_NONE                     = 1 << 0
	MOJO_READ_DATA_FLAG_DISCARD                         = 1 << 1
	MOJO_READ_DATA_FLAG_QUERY                           = 1 << 2
	MOJO_READ_DATA_FLAG_PEEK                            = 1 << 3
	MOJO_WRITE_DATA_FLAG_NONE        MojoWriteDataFlags = 0
	MOJO_WRITE_DATA_FLAG_ALL_OR_NONE MojoWriteDataFlags = 1 << 0

	MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE        MojoCreateDataPipeOptionsFlags    = 0
	MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD                                   = 1 << 0
	MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE     MojoCreateMessagePipeOptionsFlags = 0

	MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE    MojoCreateSharedBufferOptionsFlags    = 0
	MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE MojoDuplicateBufferHandleOptionsFlags = 0
	MOJO_MAP_BUFFER_FLAG_NONE                      MojoMapBufferFlags                    = 0
)

// DataPipeOptions is used to specify creation parameters for a data pipe.
type DataPipeOptions struct {
	flags MojoCreateDataPipeOptionsFlags
	// The size of an element in bytes. All transactions and buffers will
	// be an integral number of elements.
	elemSize uint32
	// The capacity of the data pipe in bytes. Must be a multiple of elemSize.
	capacity uint32
}

func (opts *DataPipeOptions) cType() *C.struct_MojoCreateDataPipeOptions {
	if opts == nil {
		return nil
	}
	var cOpts C.struct_MojoCreateDataPipeOptions
	cOpts = C.struct_MojoCreateDataPipeOptions{
		(C.uint32_t)(unsafe.Sizeof(cOpts)),
		opts.flags.cType(),
		(C.uint32_t)(opts.elemSize),
		(C.uint32_t)(opts.capacity),
	}
	return &cOpts
}

// MessagePipeOptions is used to specify creation parameters for a message pipe.
type MessagePipeOptions struct {
	flags MojoCreateMessagePipeOptionsFlags
}

func (opts *MessagePipeOptions) cType() *C.struct_MojoCreateMessagePipeOptions {
	if opts == nil {
		return nil
	}
	var cOpts C.struct_MojoCreateMessagePipeOptions
	cOpts = C.struct_MojoCreateMessagePipeOptions{
		(C.uint32_t)(unsafe.Sizeof(cOpts)),
		opts.flags.cType(),
	}
	return &cOpts
}

// SharedBufferOptions is used to specify creation parameters for a
// shared buffer.
type SharedBufferOptions struct {
	flags MojoCreateSharedBufferOptionsFlags
}

func (opts *SharedBufferOptions) cType() *C.struct_MojoCreateSharedBufferOptions {
	if opts == nil {
		return nil
	}
	var cOpts C.struct_MojoCreateSharedBufferOptions
	cOpts = C.struct_MojoCreateSharedBufferOptions{
		(C.uint32_t)(unsafe.Sizeof(cOpts)),
		opts.flags.cType(),
	}
	return &cOpts
}

// DuplicateBufferHandleOptions is used to specify parameters in
// duplicating access to a shared buffer.
type DuplicateBufferHandleOptions struct {
	flags MojoDuplicateBufferHandleOptionsFlags
}

func (opts *DuplicateBufferHandleOptions) cType() *C.struct_MojoDuplicateBufferHandleOptions {
	if opts == nil {
		return nil
	}
	var cOpts C.struct_MojoDuplicateBufferHandleOptions
	cOpts = C.struct_MojoDuplicateBufferHandleOptions{
		(C.uint32_t)(unsafe.Sizeof(cOpts)),
		opts.flags.cType(),
	}
	return &cOpts
}

// Convenience functions to convert Go types to their equivalent C types.
func (m MojoHandle) cType() C.MojoHandle {
	return (C.MojoHandle)(m)
}
func (m MojoDeadline) cType() C.MojoDeadline {
	return (C.MojoDeadline)(m)
}
func (m MojoHandleSignals) cType() C.MojoHandleSignals {
	return (C.MojoHandleSignals)(m)
}
func (m MojoWriteMessageFlags) cType() C.MojoWriteMessageFlags {
	return (C.MojoWriteMessageFlags)(m)
}
func (m MojoReadMessageFlags) cType() C.MojoReadMessageFlags {
	return (C.MojoReadMessageFlags)(m)
}
func (m MojoWriteDataFlags) cType() C.MojoWriteDataFlags {
	return (C.MojoWriteDataFlags)(m)
}
func (m MojoReadDataFlags) cType() C.MojoReadDataFlags {
	return (C.MojoReadDataFlags)(m)
}
func (m MojoCreateDataPipeOptionsFlags) cType() C.MojoCreateDataPipeOptionsFlags {
	return (C.MojoCreateDataPipeOptionsFlags)(m)
}
func (m MojoCreateMessagePipeOptionsFlags) cType() C.MojoCreateMessagePipeOptionsFlags {
	return (C.MojoCreateMessagePipeOptionsFlags)(m)
}
func (m MojoCreateSharedBufferOptionsFlags) cType() C.MojoCreateSharedBufferOptionsFlags {
	return (C.MojoCreateSharedBufferOptionsFlags)(m)
}
func (m MojoDuplicateBufferHandleOptionsFlags) cType() C.MojoDuplicateBufferHandleOptionsFlags {
	return (C.MojoDuplicateBufferHandleOptionsFlags)(m)
}
func (m MojoMapBufferFlags) cType() C.MojoMapBufferFlags {
	return (C.MojoMapBufferFlags)(m)
}
func cArrayMojoHandle(m []MojoHandle) *C.MojoHandle {
	if len(m) == 0 {
		return nil
	}
	return (*C.MojoHandle)(&m[0])
}
func cArrayMojoHandleSignals(m []MojoHandleSignals) *C.MojoHandleSignals {
	if len(m) == 0 {
		return nil
	}
	return (*C.MojoHandleSignals)(&m[0])
}
func cArrayBytes(m []byte) unsafe.Pointer {
	if len(m) == 0 {
		return nil
	}
	return unsafe.Pointer(&m[0])
}

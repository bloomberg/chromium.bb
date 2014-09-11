# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=F0401

import interface_dsl

def MakeInterface():
  mojo = interface_dsl.Interface()

  f = mojo.Func('MojoCreateSharedBuffer', 'MojoResult')
  f.Param('options').InStruct('MojoCreateSharedBufferOptions').Optional()
  f.Param('num_bytes').In('uint64_t')
  f.Param('shared_buffer_handle').Out('MojoHandle')

  f = mojo.Func('MojoDuplicateBufferHandle', 'MojoResult')
  f.Param('buffer_handle').In('MojoHandle')
  f.Param('options').InStruct('MojoDuplicateBufferHandleOptions').Optional()
  f.Param('new_buffer_handle').Out('MojoHandle')

  f = mojo.Func('MojoMapBuffer', 'MojoResult')
  f.Param('buffer_handle').In('MojoHandle')
  f.Param('offset').In('uint64_t')
  f.Param('num_bytes').In('uint64_t')
  f.Param('buffer').Out('void*')
  f.Param('flags').In('MojoMapBufferFlags')

  f = mojo.Func('MojoUnmapBuffer', 'MojoResult')
  f.Param('buffer').In('void*')

  f = mojo.Func('MojoCreateDataPipe', 'MojoResult')
  f.Param('options').InStruct('MojoCreateDataPipeOptions').Optional()
  f.Param('data_pipe_producer_handle').Out('MojoHandle')
  f.Param('data_pipe_consumer_handle').Out('MojoHandle')

  f = mojo.Func('MojoWriteData', 'MojoResult')
  f.Param('data_pipe_producer_handle').In('MojoHandle')
  f.Param('elements').InArray('void', 'num_bytes')
  f.Param('num_bytes').InOut('uint32_t')
  f.Param('flags').In('MojoWriteDataFlags')

  f = mojo.Func('MojoBeginWriteData', 'MojoResult')
  f.Param('data_pipe_producer_handle').In('MojoHandle')
  f.Param('buffer').Out('void*')
  f.Param('buffer_num_bytes').InOut('uint32_t')
  f.Param('flags').In('MojoWriteDataFlags')

  f = mojo.Func('MojoEndWriteData', 'MojoResult')
  f.Param('data_pipe_producer_handle').In('MojoHandle')
  f.Param('num_bytes_written').In('uint32_t')

  f = mojo.Func('MojoReadData', 'MojoResult')
  f.Param('data_pipe_consumer_handle').In('MojoHandle')
  f.Param('elements').OutArray('void', 'num_bytes')
  f.Param('num_bytes').InOut('uint32_t')
  f.Param('flags').In('MojoReadDataFlags')

  f = mojo.Func('MojoBeginReadData', 'MojoResult')
  f.Param('data_pipe_consumer_handle').In('MojoHandle')
  f.Param('buffer').Out('const void*')
  f.Param('buffer_num_bytes').InOut('uint32_t')
  f.Param('flags').In('MojoReadDataFlags')

  f = mojo.Func('MojoEndReadData', 'MojoResult')
  f.Param('data_pipe_consumer_handle').In('MojoHandle')
  f.Param('num_bytes_read').In('uint32_t')

  f = mojo.Func('MojoGetTimeTicksNow', 'MojoTimeTicks')

  f = mojo.Func('MojoClose', 'MojoResult')
  f.Param('handle').In('MojoHandle')

  f = mojo.Func('MojoWait', 'MojoResult')
  f.Param('handle').In('MojoHandle')
  f.Param('signals').In('MojoHandleSignals')
  f.Param('deadline').In('MojoDeadline')

  f = mojo.Func('MojoWaitMany', 'MojoResult')
  f.Param('handles').InArray('MojoHandle', 'num_handles')
  f.Param('signals').InArray('MojoHandleSignals', 'num_handles')
  f.Param('num_handles').In('uint32_t')
  f.Param('deadline').In('MojoDeadline')

  f = mojo.Func('MojoCreateMessagePipe', 'MojoResult')
  f.Param('options').InStruct('MojoCreateMessagePipeOptions').Optional()
  f.Param('message_pipe_handle0').Out('MojoHandle')
  f.Param('message_pipe_handle1').Out('MojoHandle')

  f = mojo.Func('MojoWriteMessage', 'MojoResult')
  f.Param('message_pipe_handle').In('MojoHandle')
  f.Param('bytes').InArray('void', 'num_bytes').Optional()
  f.Param('num_bytes').In('uint32_t')
  f.Param('handles').InArray('MojoHandle', 'num_handles').Optional()
  f.Param('num_handles').In('uint32_t')
  f.Param('flags').In('MojoWriteMessageFlags')

  f = mojo.Func('MojoReadMessage', 'MojoResult')
  f.Param('message_pipe_handle').In('MojoHandle')
  f.Param('bytes').OutArray('void', 'num_bytes').Optional()
  f.Param('num_bytes').InOut('uint32_t').Optional()
  f.Param('handles').OutArray('MojoHandle', 'num_handles').Optional()
  f.Param('num_handles').InOut('uint32_t').Optional()
  f.Param('flags').In('MojoReadMessageFlags')

  mojo.Finalize()

  return mojo

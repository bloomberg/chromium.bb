# Mojo C System API
This document is a subset of the [Mojo documentation](/mojo).

[TOC]

## Overview
The Mojo C System API is a lightweight API (with an eventually-stable ABI) upon
which all higher layers of the Mojo system are built.

This API exposes the fundamental capabilities to: create, read from, and write
to **message pipes**; create, read from, and write to **data pipes**; create
**shared buffers** and generate sharable handles to them; wrap platform-specific
handle objects (such as **file descriptors**, **Windows handles**, and
**Mach ports**) for seamless transit over message pipes; and efficiently watch
handles for various types of state transitions.

This document provides a brief guide to API usage with example code snippets.
For a detailed API references please consult the headers in
[//mojo/public/c/system](https://cs.chromium.org/chromium/src/mojo/public/c/system/).

### A Note About Multithreading

The Mojo C System API is entirely thread-agnostic. This means that all functions
may be called from any thread in a process, and there are no restrictions on how
many threads can use the same object at the same time.

Of course this does not mean you can completely ignore potential concurrency
issues -- such as a handle being closed on one thread while another thread is
trying to perform an operation on the same handle -- but there is nothing
fundamentally incorrect about using any given API or handle from multiple
threads.

### A Note About Synchronization

Every Mojo API call is non-blocking and synchronously yields some kind of status
result code, but the call's side effects -- such as affecting the state of
one or more handles in the system -- may or may not occur asynchronously.

Mojo objects can be observed for interesting state changes in a way that is
thread-agnostic and in some ways similar to POSIX signal handlers: *i.e.*
user-provided notification handlers may be invoked at any time on arbitrary
threads in the process. It is entirely up to the API user to take appropriate
measures to synchronize operations against other application state.

The higher level [system](/mojo#High-Level-System-APIs) and
[bindings](/mojo#High-Level-Bindings-APIs) APIs provide helpers to simplify Mojo
usage in this regard, at the expense of some flexibility.

## Result Codes

Most API functions return a value of type `MojoResult`. This is an integral
result code used to convey some meaningful level of detail about the result of a
requested operation.

See [//mojo/public/c/system/types.h](https://cs.chromium.org/chromium/src/mojo/public/c/system/types.h)
for different possible values. See documentation for individual API calls for
more specific contextual meaning of various result codes.

## Handles

Every Mojo IPC primitive is identified by a generic, opaque integer handle of
type `MojoHandle`. Handles can be acquired by creating new objects using various
API calls, or by reading messages which contain attached handles.

A `MojoHandle` can represent a message pipe endpoint, a data pipe consumer,
a data pipe producer, a shared buffer reference, a wrapped native platform
handle such as a POSIX file descriptor or a Windows system handle, or a watcher
object (see [Signals & Watchers](#Signals-Watchers) below.)

All types of handles except for watchers (which are an inherently local concept)
can be attached to messages and sent over message pipes.

Any `MojoHandle` may be closed by calling `MojoClose`:

``` c
MojoHandle x = DoSomethingToGetAValidHandle();
MojoResult result = MojoClose(x);
```

If the handle passed to `MojoClose` was a valid handle, it will be closed and
`MojoClose` returns `MOJO_RESULT_OK`. Otherwise it returns
`MOJO_RESULT_INVALID_ARGUMENT`.

Similar to native system handles on various popular platforms, `MojoHandle`
values may be reused over time. Thus it is important to avoid logical errors
which lead to misplaced handle ownership, double-closes, *etc.*

## Message Pipes

A message pipe is a bidirectional messaging channel which can carry arbitrary
unstructured binary messages with zero or more `MojoHandle` attachments to be
transferred from one end of a pipe to the other. Message pipes work seamlessly
across process boundaries or within a single process.

The [Embedder Development Kit (EDK)](/mojo/edk/embedder) provides the means to
bootstrap one or more primordial cross-process message pipes, and it's up to
Mojo embedders to expose this capability in some useful way. Once such a pipe is
established, additional handles -- including other message pipe handles -- may
be sent to a remote process using that pipe (or in turn, over other pipes sent
over that pipe, or pipes sent over *that* pipe, and so on...)

The public C System API exposes the ability to read and write messages on pipes
and to create new message pipes.

See [//mojo/public/c/system/message_pipe.h](https://cs.chromium.org/chromium/src/mojo/public/c/system/message_pipe.h)
for detailed message pipe API documentation.

### Creating Message Pipes

`MojoCreateMessagePipe` can be used to create a new message pipe:

``` c
MojoHandle a, b;
MojoResult result = MojoCreateMessagePipe(NULL, &a, &b);
```

After this snippet, `result` should be `MOJO_RESULT_OK` (it's really hard for
this to fail!), and `a` and `b` will contain valid Mojo handles, one for each
end of the new message pipe.

Any messages written to `a` are eventually readable from `b`, and any messages
written to `b` are eventually readable from `a`. If `a` is closed at any point,
`b` will eventually become aware of this fact; likewise if `b` is closed, `a`
will become aware of that.

The state of these conditions can be queried and watched asynchronously as
described in the [Signals & Watchers](#Signals-Watchers) section below.

### Creating Messages

Message pipes carry message objects which may or may not be serialized. You can
create a new message object as follows:

``` c
MojoMessageHandle message;
MojoResult result = MojoCreateMessage(&message);
```

Note that we have a special `MojoMessageHandle` type for message objects.

Messages may be serialized or unserialized. Unserialized messages may support
lazy serialization, meaning they can be serialized if and only if necessary
(e.g. if the message needs to cross a process boundary.)

To make a serialized message, you might write something like:

``` c
void* buffer;
uint32_t buffer_size;
MojoResult result = MojoAttachSerializedMessageBuffer(
    message, 6, nullptr, 0, &buffer, &buffer_size);
```

This attaches a serialized message buffer to `message` with at least `6` bytes
of storage capacity. The results stored in `buffer` and `buffer_size` give more
information about the payload's storage.

If you want to increase the size of the payload layer, you can use
`MojoExtendSerializedMessagePayload`.

Creating lazily-serialized messages is also straightforward:

``` c
struct MyMessage {
  // some interesting data...
};

void SerializeMessage(MojoMessageHandle message, uintptr_t context) {
  struct MyMessage* my_message = (struct MyMessage*)context;

  MojoResult result = MojoAttachSerializedMessageBuffer(message, ...);
  // Serialize however you like.

  free(my_message);
}

MyMessage* data = malloc(sizeof(MyMessage));
// initialize *data...

MojoResult result = MojoAttachMessageContext(message, (uintptr_t)data,
                                             &SerializeMessage);
```

If we change our mind and decide not to send a message, we can destroy it:

``` c
MojoResult result = MojoDestroyMessage(message);
```

Note that attempting to write a message transfers ownership of the message
object (and any attached handles) into the message pipe, and there is therefore
no need to subsequently call `MojoDestroyMessage` on that message.

### Writing Messages

``` c
result = MojoWriteMessage(a, message, MOJO_WRITE_MESSAGE_FLAG_NONE);
```

`MojoWriteMessage` is a *non-blocking* call: it always returns
immediately. If its return code is `MOJO_RESULT_OK` the message will eventually
find its way to the other end of the pipe -- assuming that end isn't closed
first, of course. If the return code is anything else, the message is deleted
and not transferred.

In this case since we know `b` is still open, we also know the message will
eventually arrive at `b`. `b` can be queried or watched to become aware of when
the message arrives, but we'll ignore that complexity for now. See
[Signals & Watchers](#Signals-Watchers) below for more information.

*** aside
**NOTE**: Although this is an implementation detail and not strictly guaranteed
by the System API, it is true in the current implementation that the message
will arrive at `b` before the above `MojoWriteMessage` call even returns,
because `b` is in the same process as `a` and has never been transferred over
another pipe.
***

### Reading Messages

We can read a new message object from a pipe:

``` c
MojoMessageHandle message;
MojoResult result = MojoReadMessage(b, &message, MOJO_READ_MESSAGE_FLAG_NONE);
```

and extract its serialized contents:

``` c
void* buffer = NULL;
uint32_t num_bytes;
MojoResult result = MojoGetSerializedMessageContents(
    message, &buffer, &num_bytes, nullptr, nullptr,
    MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
printf("Pipe says: %s", (const char*)buffer);
```

`result` should be `MOJO_RESULT_OK` and this snippet should write `"hello"` to
`stdout`.

If we try were to try reading again now that there are no messages on `b`:

``` c
MojoMessageHandle message;
MojoResult result = MojoReadMessage(b, &message, MOJO_READ_MESSAGE_FLAG_NONE);
```

We'll get a `result` of `MOJO_RESULT_SHOULD_WAIT`, indicating that the pipe is
not yet readable.

Note that message also may not have been serialized if they came from within the
same process. In that case, `MojoGetSerializedMessageContents` will return
`MOJO_RESULT_FAILED_PRECONDITION`. The message's unserialized context can
instead be retrieved using `MojoGetMessageContext`.

Messages read from a message pipe are owned by the caller and must be
subsequently destroyed using `MojoDestroyMessage` (or, in theory, written to
another pipe using `MojoWriteMessage`.)

### Messages With Handles

Probably the most useful feature of Mojo IPC is that message pipes can carry
arbitrary Mojo handles, including other message pipes. This is also
straightforward.

Here's an example which creates two pipes, using the first pipe to transfer
one end of the second pipe. If you have a good imagination you can pretend the
first pipe spans a process boundary, which makes the example more practically
interesting:

``` c
MojoHandle a, b;
MojoHandle c, d;
MojoCreateMessagePipe(NULL, &a, &b);
MojoCreateMessagePipe(NULL, &c, &d);

// Allocate a message with an empty payload and handle |c| attached. Note that
// this takes ownership of |c|, effectively invalidating its handle value.
MojoMessageHandle message;
void* buffer;
uint32_t buffer_size;
MojoCreateMessage(&message);
MojoAttachSerializedMessageBuffer(message, "hi", 2, &c, 1, &buffer,
                                  &buffer_size);
MojoWriteMessage(a, message, MOJO_WRITE_MESSAGE_FLAG_NONE);

// Some time later...
MojoHandle e;
uint32_t num_handles = 1;
MojoReadMessage(b, &message, MOJO_READ_MESSAGE_FLAG_NONE);
MojoGetSerializedMessageContents(
    message, &buffer, &buffer_size, &e, &num_handles,
    MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
```

At this point the handle in `e` is now referencing the same message pipe
endpoint which was originally referenced by `c`.

Note that `num_handles` above is initialized to 1 before we pass its address to
`MojoGetSerializedMessageContents`. This is to indicate how much `MojoHandle`
storage is available at the output buffer we gave it (`&e` above).

If we didn't know how many handles to expect in an incoming message -- which is
often the case -- we can use `MojoGetSerializedMessageContents` to query for
this information first:

``` c
MojoMessageHandle message;
void* buffer;
uint32_t num_bytes = 0;
uint32_t num_handles = 0;
MojoResult result = MojoGetSerializedMessageContents(
    message, &buffer, &num_bytes, NULL, &num_handles,
    MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
```

If `message` has some non-zero number of handles, `result` will be
`MOJO_RESULT_RESOURCE_EXHAUSTED`, and both `num_bytes` and `num_handles` would
be updated to reflect the payload size and number of attached handles in the
message.

## Data Pipes

Data pipes provide an efficient unidirectional channel for moving large amounts
of unframed data between two endpoints. Every data pipe has a fixed
**element size** and **capacity**. Reads and writes must be done in sizes that
are a multiple of the element size, and writes to the pipe can only be queued
up to the pipe's capacity before reads must be done to make more space
available.

Every data pipe has a single **producer** handle used to write data into the
pipe and a single **consumer** handle used to read data out of the pipe.

Finally, data pipes support both immediate I/O -- reading into and writing out
from user-supplied buffers -- as well as two-phase I/O, allowing callers to
temporarily lock some portion of the data pipe in order to read or write its
contents directly.

See [//mojo/public/c/system/data_pipe.h](https://cs.chromium.org/chromium/src/mojo/public/c/system/data_pipe.h)
for detailed data pipe API documentation.

### Creating Data Pipes

Use `MojoCreateDataPipe` to create a new data pipe. The
`MojoCreateDataPipeOptions` structure is used to configure the new pipe, but
this can be omitted to assume the default options of a single-byte element size
and an implementation-defined default capacity (64 kB at the time of this
writing.)

``` c
MojoHandle producer, consumer;
MojoResult result = MojoCreateDataPipe(NULL, &producer, &consumer);
```

### Immediate I/O

Data can be written into or read out of a data pipe using buffers provided by
the caller. This is generally more convenient than two-phase I/O but is
also less efficient due to extra copying.

``` c
uint32_t num_bytes = 12;
MojoResult result = MojoWriteData(producer, "datadatadata", &num_bytes,
                                  MOJO_WRITE_DATA_FLAG_NONE);
```

The above snippet will attempt to write 12 bytes into the data pipe, which
should succeed and return `MOJO_RESULT_OK`. If the available capacity on the
pipe was less than the amount requested (the input value of `*num_bytes`) this
will copy what it can into the pipe and return the number of bytes written in
`*num_bytes`. If no data could be copied this will instead return
`MOJO_RESULT_SHOULD_WAIT`.

Reading from the consumer is a similar operation.

``` c
char buffer[64];
uint32_t num_bytes = 64;
MojoResult result = MojoReadData(consumer, buffer, &num_bytes,
                                 MOJO_READ_DATA_FLAG_NONE);
```

This will attempt to read up to 64 bytes, returning the actual number of bytes
read in `*num_bytes`.

`MojoReadData` supports a number of interesting flags to change the behavior:
you can peek at the data (copy bytes out without removing them from the pipe),
query the number of bytes available without doing any actual reading of the
contents, or discard data from the pipe without bothering to copy it anywhere.

This also supports a `MOJO_READ_DATA_FLAG_ALL_OR_NONE` which ensures that the
call succeeds **only** if the exact number of bytes requested could be read.
Otherwise such a request will fail with `MOJO_READ_DATA_OUT_OF_RANGE`.

### Two-Phase I/O

Data pipes also support two-phase I/O operations, allowing a caller to
temporarily lock a portion of the data pipe's storage for direct memory access.

``` c
void* buffer;
uint32_t num_bytes = 1024;
MojoResult result = MojoBeginWriteData(producer, &buffer, &num_bytes,
                                       MOJO_WRITE_DATA_FLAG_NONE);
```

This requests write access to a region of up to 1024 bytes of the data pipe's
next available capacity. Upon success, `buffer` will point to the writable
storage and `num_bytes` will indicate the size of the buffer there.

The caller should then write some data into the memory region and release it
ASAP, indicating the number of bytes actually written:

``` c
memcpy(buffer, "hello", 6);
MojoResult result = MojoEndWriteData(producer, 6);
```

Two-phase reads look similar:

``` c
void* buffer;
uint32_t num_bytes = 1024;
MojoResult result = MojoBeginReadData(consumer, &buffer, &num_bytes,
                                      MOJO_READ_DATA_FLAG_NONE);
// result should be MOJO_RESULT_OK, since there is some data available.

printf("Pipe says: %s", (const char*)buffer);  // Should say "hello".

result = MojoEndReadData(consumer, 1);  // Say we only consumed one byte.

num_bytes = 1024;
result = MojoBeginReadData(consumer, &buffer, &num_bytes,
                           MOJO_READ_DATA_FLAG_NONE);
printf("Pipe says: %s", (const char*)buffer);  // Should say "ello".
result = MojoEndReadData(consumer, 5);
```

## Shared Buffers

Shared buffers are chunks of memory which can be mapped simultaneously by
multiple processes. Mojo provides a simple API to make these available to
applications.

See [//mojo/public/c/system/buffer.h](https://cs.chromium.org/chromium/src/mojo/public/c/system/buffer.h)
for detailed shared buffer API documentation.

### Creating Buffer Handles

Usage is straightforward. You can create a new buffer:

``` c
// Allocate a shared buffer of 4 kB.
MojoHandle buffer;
MojoResult result = MojoCreateSharedBuffer(NULL, 4096, &buffer);
```

You can also duplicate an existing shared buffer handle:

``` c
MojoHandle another_name_for_buffer;
MojoResult result = MojoDuplicateBufferHandle(buffer, NULL,
                                              &another_name_for_buffer);
```

This is useful if you want to retain a handle to the buffer while also sharing
handles with one or more other clients. The allocated buffer remains valid as
long as at least one shared buffer handle exists to reference it.

### Mapping Buffers

You can map (and later unmap) a specified range of the buffer to get direct
memory access to its contents:

``` c
void* data;
MojoResult result = MojoMapBuffer(buffer, 0, 64, &data,
                                  MOJO_MAP_BUFFER_FLAG_NONE);

*(int*)data = 42;
result = MojoUnmapBuffer(data);
```

A buffer may have any number of active mappings at a time, in any number of
processes.

### Read-Only Handles

An option can also be specified on `MojoDuplicateBufferHandle` to ensure
that the newly duplicated handle can only be mapped to read-only memory:

``` c
MojoHandle read_only_buffer;
MojoDuplicateBufferHandleOptions options;
options.struct_size = sizeof(options);
options.flags = MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_READ_ONLY;
MojoResult result = MojoDuplicateBufferHandle(buffer, &options,
                                              &read_only_buffer);

// Attempt to map and write to the buffer using the read-only handle:
void* data;
result = MojoMapBuffer(read_only_buffer, 0, 64, &data,
                       MOJO_MAP_BUFFER_FLAG_NONE);
*(int*)data = 42;  // CRASH
```

*** note
**NOTE:** One important limitation of the current implementation is that
read-only handles can only be produced from a handle that was originally created
by `MojoCreateSharedBuffer` (*i.e.*, you cannot create a read-only duplicate
from a non-read-only duplicate), and the handle cannot have been transferred
over a message pipe first.
***

## Native Platform Handles (File Descriptors, Windows Handles, *etc.*)

Native platform handles to system objects can be wrapped as Mojo handles for
seamless transit over message pipes. Mojo currently supports wrapping POSIX
file descriptors, Windows handles, and Mach ports.

See [//mojo/public/c/system/platform_handle.h](https://cs.chromium.org/chromium/src/mojo/public/c/system/platform_handle.h)
for detailed platform handle API documentation.

### Wrapping Basic Handle Types

Wrapping a POSIX file descriptor is simple:

``` c
MojoPlatformHandle platform_handle;
platform_handle.struct_size = sizeof(platform_handle);
platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
platform_handle.value = (uint64_t)fd;
MojoHandle handle;
MojoResult result = MojoWrapPlatformHandle(&platform_handle, &handle);
```

Note that at this point `handle` effectively owns the file descriptor
and if you were to call `MojoClose(handle)`, the file descriptor would be closed
too; but we're not going to close it here! We're going to pretend we've sent it
over a message pipe, and now we want to unwrap it on the other side:

``` c
MojoPlatformHandle platform_handle;
platform_handle.struct_size = sizeof(platform_handle);
MojoResult result = MojoUnwrapPlatformHandle(handle, &platform_handle);
int fd = (int)platform_handle.value;
```

The situation looks nearly identical for wrapping and unwrapping Windows handles
and Mach ports.

### Wrapping Shared Buffer Handles

Unlike other handle types, shared buffers have special meaning in Mojo, and it
may be desirable to wrap a native platform handle -- along with some extra
metadata -- such that be treated like a real Mojo shared buffer handle.
Conversely it can also be useful to unpack a Mojo shared buffer handle into
a native platform handle which references the buffer object. Both of these
things can be done using the `MojoWrapPlatformSharedBuffer` and
`MojoUnwrapPlatformSharedBuffer` APIs.

On Windows, the wrapped platform handle must always be a Windows handle to
a file mapping object.

On OS X, the wrapped platform handle must be a memory-object send right.

On all other POSIX systems, the wrapped platform handle must be a file
descriptor for a shared memory object.

## Signals & Watchers

Message pipe and data pipe (producer and consumer) handles can change state in
ways that may be interesting to a Mojo API user. For example, you may wish to
know when a message pipe handle has messages available to be read or when its
peer has been closed. Such states are reflected by a fixed set of boolean
signals on each pipe handle.

### Signals

Every message pipe and data pipe handle maintains a notion of
**signaling state** which may be queried at any time. For example:

``` c
MojoHandle a, b;
MojoCreateMessagePipe(NULL, &a, &b);

MojoHandleSignalsState state;
MojoResult result = MojoQueryHandleSignalsState(a, &state);
```

The `MojoHandleSignalsState` structure exposes two fields: `satisfied_signals`
and `satisfiable_signals`. Both of these are bitmasks of the type
`MojoHandleSignals` (see [//mojo/public/c/system/types.h](https://cs.chromium.org/chromium/src/mojo/public/c/system/types.h)
for more details.)

The `satisfied_signals` bitmask indicates signals which were satisfied on the
handle at the time of the call, while the `satisfiable_signals` bitmask
indicates signals which were still possible to satisfy at the time of the call.
It is thus by definition always true that:

``` c
(satisfied_signals | satisfiable_signals) == satisfiable_signals
```

In other words a signal obviously cannot be satisfied if it is no longer
satisfiable. Furthermore once a signal is unsatisfiable, *i.e.* is no longer
set in `sastisfiable_signals`, it can **never** become satisfiable again.

To illustrate this more clearly, consider the message pipe created above. Both
ends of the pipe are still open and neither has been written to yet. Thus both
handles start out with the same signaling state:

| Field                 | State |
|-----------------------|-------|
| `satisfied_signals`   | `MOJO_HANDLE_SIGNAL_WRITABLE`
| `satisfiable_signals` | `MOJO_HANDLE_SIGNAL_READABLE + MOJO_HANDLE_SIGNAL_WRITABLE + MOJO_HANDLE_SIGNAL_PEER_CLOSED`

Writing a message to handle `b` will eventually alter the signaling state of `a`
such that `MOJO_HANDLE_SIGNAL_READABLE` also becomes satisfied. If we were to
then close `b`, the signaling state of `a` would look like:

| Field                 | State |
|-----------------------|-------|
| `satisfied_signals`   | `MOJO_HANDLE_SIGNAL_READABLE + MOJO_HANDLE_SIGNAL_PEER_CLOSED`
| `satisfiable_signals` | `MOJO_HANDLE_SIGNAL_READABLE + MOJO_HANDLE_SIGNAL_PEER_CLOSED`

Note that even though `a`'s peer is known to be closed (hence making `a`
permanently unwritable) it remains readable because there's still an unread
received message waiting to be read from `a`.

Finally if we read the last message from `a` its signaling state becomes:

| Field                 | State |
|-----------------------|-------|
| `satisfied_signals`   | `MOJO_HANDLE_SIGNAL_PEER_CLOSED`
| `satisfiable_signals` | `MOJO_HANDLE_SIGNAL_PEER_CLOSED`

and we know definitively that `a` can never be read from again.

### Watching Signals

The ability to query a handle's signaling state can be useful, but it's not
sufficient to support robust and efficient pipe usage. Mojo watchers empower
users with the ability to **watch** a handle's signaling state for interesting
changes and automatically invoke a notification handler in response.

When a watcher is created it must be bound to a function pointer matching
the following signature, defined in
[//mojo/public/c/system/watcher.h](https://cs.chromium.org/chromium/src/mojo/public/c/system/watcher.h):

``` c
typedef void (*MojoWatcherNotificationCallback)(
    uintptr_t context,
    MojoResult result,
    MojoHandleSignalsState signals_state,
    MojoWatcherNotificationFlags flags);
```

The `context` argument corresponds to a specific handle being watched by the
watcher (read more below), and the remaining arguments provide details regarding
the specific reason for the notification. It's important to be aware that a
watcher's registered handler may be called **at any time** and
**on any thread**.

It's also helpful to understand a bit about the mechanism by which the handler
can be invoked. Essentially, any Mojo C System API call may elicit a handle
state change of some kind. If such a change is relevant to conditions watched by
a watcher, and that watcher is in a state which allows it raise a corresponding
notification, its notification handler will be invoked synchronously some time
before the outermost System API call on the current thread's stack returns.

Handle state changes can also occur as a result of incoming IPC from an external
process. If a pipe in the current process is connected to an endpoint in another
process and the internal Mojo system receives an incoming message bound for the
local endpoint, the arrival of that message will trigger a state change on the
receiving handle and may thus invoke one or more watchers' notification handlers
as a result.

The `MOJO_WATCHER_NOTIFICATION_FLAG_FROM_SYSTEM` flag on the notification
handler's `flags` argument is used to indicate whether the handler was invoked
due to such an internal system IPC event (if the flag is set), or if it was
invoked synchronously due to some local API call (if the flag is unset.)
This distinction can be useful to make in certain cases to *e.g.* avoid
accidental reentrancy in user code.

### Creating a Watcher

Creating a watcher is simple:

``` c

void OnNotification(uintptr_t context,
                    MojoResult result,
                    MojoHandleSignalsState signals_state,
                    MojoWatcherNotificationFlags flags) {
  // ...
}

MojoHandle w;
MojoResult result = MojoCreateWatcher(&OnNotification, &w);
```

Like all other `MojoHandle` types, watchers may be destroyed by closing them
with `MojoClose`. Unlike other `MojoHandle` types, watcher handles are **not**
transferrable across message pipes.

In order for a watcher to be useful, it has to watch at least one handle.

### Adding a Handle to a Watcher

Any given watcher can watch any given (message or data pipe) handle for some set
of signaling conditions. A handle may be watched simultaneously by multiple
watchers, and a single watcher can watch multiple different handles
simultaneously.

``` c
MojoHandle a, b;
MojoCreateMessagePipe(NULL, &a, &b);

// Watch handle |a| for readability.
const uintptr_t context = 1234;
MojoResult result = MojoWatch(w, a, MOJO_HANDLE_SIGNAL_READABLE, context);
```

We've successfully instructed watcher `w` to begin watching pipe handle `a` for
readability. However, our recently created watcher is still in a **disarmed**
state, meaning that it will never fire a notification pertaining to this watched
signaling condition. It must be **armed** before that can happen.

### Arming a Watcher

In order for a watcher to invoke its notification handler in response to a
relevant signaling state change on a watched handle, it must first be armed. A
watcher may only be armed if none of its watched handles would elicit a
notification immediately once armed.

In this case `a` is clearly not yet readable, so arming should succeed:

``` c
MojoResult result = MojoArmWatcher(w, NULL, NULL, NULL, NULL);
```

Now we can write to `b` to make `a` readable:

``` c
MojoWriteMessage(b, NULL, 0, NULL, 0, MOJO_WRITE_MESSAGE_NONE);
```

Eventually -- and in practice possibly before `MojoWriteMessage` even
returns -- this will cause `OnNotification` to be invoked on the calling thread
with the `context` value (*i.e.* 1234) that was given when the handle was added
to the watcher.

The `result` parameter will be `MOJO_RESULT_OK` to indicate that the watched
signaling condition has been *satisfied*. If the watched condition had instead
become permanently *unsatisfiable* (*e.g.*, if `b` were instead closed), `result`
would instead indicate `MOJO_RESULT_FAILED_PRECONDITION`.

**NOTE:** Immediately before a watcher decides to invoke its notification
handler, it automatically disarms itself to prevent another state change from
eliciting another notification. Therefore a watcher must be repeatedly rearmed
in order to continue dispatching signaling notifications.

As noted above, arming a watcher may fail if any of the watched conditions for
a handle are already partially satisfied or fully unsatisfiable. In that case
the caller may provide buffers for `MojoArmWatcher` to store information about
a subset of the relevant watches which caused it to fail:

``` c
// Provide some storage for information about watches that are already ready.
uint32_t num_ready_contexts = 4;
uintptr_t ready_contexts[4];
MojoResult ready_results[4];
struct MojoHandleSignalsStates ready_states[4];
MojoResult result = MojoArmWatcher(w, &num_ready_contexts, ready_contexts,
                                   ready_results, ready_states);
```

Because `a` is still readable this operation will fail with
`MOJO_RESULT_FAILED_PRECONDITION`. The input value of `num_ready_contexts`
informs `MojoArmWatcher` that it may store information regarding up to 4 watches
which currently prevent arming. In this case of course there is only one active
watch, so upon return we will see:

* `num_ready_contexts` is `1`.
* `ready_contexts[0]` is `1234`.
* `ready_results[0]` is `MOJO_RESULT_OK`
* `ready_states[0]` is the last known signaling state of handle `a`.

In other words the stored information mirrors what would have been the
notification handler's arguments if the watcher were allowed to arm and thus
notify immediately.

### Cancelling a Watch

There are three ways a watch can be cancelled:

* The watched handle is closed
* The watcher handle is closed (in which case all of its watches are cancelled.)
* `MojoCancelWatch` is explicitly called for a given `context`.

In the above example this means any of the following operations will cancel the
watch on `a`:

``` c
// Close the watched handle...
MojoClose(a);

// OR close the watcher handle...
MojoClose(w);

// OR explicitly cancel.
MojoResult result = MojoCancelWatch(w, 1234);
```

In every case the watcher's notification handler is invoked for the cancelled
watch(es) regardless of whether or not the watcher is or was armed at the time.
The notification handler receives a `result` of `MOJO_RESULT_CANCELLED` for
these notifications, and this is guaranteed to be the final notification for any
given watch context.

### Practical Watch Context Usage

It is common and probably wise to treat a watch's `context` value as an opaque
pointer to some thread-safe state associated in some way with the handle being
watched. Here's a small example which uses a single watcher to watch both ends
of a message pipe and accumulate a count of messages received at each end.

``` c
// NOTE: For the sake of simplicity this example code is not in fact
// thread-safe. As long as there's only one thread running in the process and
// no external process connections, this is fine.

struct WatchedHandleState {
  MojoHandle watcher;
  MojoHandle handle;
  int message_count;
};

void OnNotification(uintptr_t context,
                    MojoResult result,
                    MojoHandleSignalsState signals_state,
                    MojoWatcherNotificationFlags flags) {
  struct WatchedHandleState* state = (struct WatchedHandleState*)(context);
  MojoResult rv;

  if (result == MOJO_RESULT_CANCELLED) {
    // Cancellation is always the last notification and is guaranteed to
    // eventually happen for every context, assuming no handles are leaked. We
    // treat this as an opportunity to free the WatchedHandleState.
    free(state);
    return;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // No longer readable, i.e. the other handle must have been closed. Better
    // cancel. Note that we could also just call MojoClose(state->watcher) here
    // since we know |context| is its only registered watch.
    MojoCancelWatch(state->watcher, context);
    return;
  }

  // This is the only handle watched by the watcher, so as long as we can't arm
  // the watcher we know something's up with this handle. Try to read messages
  // until we can successfully arm again or something goes terribly wrong.
  while (MojoArmWatcher(state->watcher, NULL, NULL, NULL, NULL) ==
         MOJO_RESULT_FAILED_PRECONDITION) {
    rv = MojoReadMessageNew(state->handle, NULL, NULL, NULL,
                            MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
    if (rv == MOJO_RESULT_OK) {
      state->message_count++;
    } else if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
      MojoCancelWatch(state->watcher, context);
      return;
    }
  }
}

MojoHandle a, b;
MojoCreateMessagePipe(NULL, &a, &b);

MojoHandle a_watcher, b_watcher;
MojoCreateWatcher(&OnNotification, &a_watcher);
MojoCreateWatcher(&OnNotification, &b_watcher)

struct WatchedHandleState* a_state = malloc(sizeof(struct WatchedHandleState));
a_state->watcher = a_watcher;
a_state->handle = a;
a_state->message_count = 0;

struct WatchedHandleState* b_state = malloc(sizeof(struct WatchedHandleState));
b_state->watcher = b_watcher;
b_state->handle = b;
b_state->message_count = 0;

MojoWatch(a_watcher, a, MOJO_HANDLE_SIGNAL_READABLE, (uintptr_t)a_state);
MojoWatch(b_watcher, b, MOJO_HANDLE_SIGNAL_READABLE, (uintptr_t)b_state);

MojoArmWatcher(a_watcher, NULL, NULL, NULL, NULL);
MojoArmWatcher(b_watcher, NULL, NULL, NULL, NULL);
```

Now any writes to `a` will increment `message_count` in `b_state`, and any
writes to `b` will increment `message_count` in `a_state`.

If either `a` or `b` is closed, both watches will be cancelled - one because
watch cancellation is implicit in handle closure, and the other because its
watcher will eventually detect that the handle is no longer readable.

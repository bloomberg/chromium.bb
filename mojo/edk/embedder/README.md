# Mojo Embedder Development Kit (EDK)
This document is a subset of the [Mojo documentation](/mojo/README.md).

[TOC]

## Overview

The Mojo EDK is a (binary-unstable) API which enables a process to initialize
and use Mojo for IPC to other processes. Note that this library is only usable
when statically linking the EDK into your application. Applications which want
to use a shared external copy of the Mojo implementation should instead call
`MojoInitialize()` to initialize Mojo and IPC. See the note about `mojo_core`
[here](/mojo/README.md#Mojo-Core-Library).

Using any of the API surface in `//mojo/edk/embedder` requires a direct
dependency on the GN `//mojo/edk` target. Headers in `mojo/edk/system` are
reserved for internal use by the EDK only.

**NOTE:** Unless you are introducing a new binary entry point into the system
(*e.g.,* a new executable with a new `main()` definition), you probably don't
need to know anything about the EDK API. Most processes defined in the Chrome
repo today already fully initialize the EDK so that Mojo's other public APIs
"just work" out of the box.

## Basic Initialization

In order to use Mojo in an application statically linking the EDK, it's
necessary to call `mojo::edk::Init` exactly once:

```
#include "mojo/edk/embedder/embedder.h"

int main(int argc, char** argv) {
  mojo::edk::Init();

  // Now you can create message pipes, write messages, etc

  return 0;
}
```

This enables local API calls to work, so message pipes *etc* can be created and
used. In some cases (particuarly many unit testing scenarios) this is
sufficient, but to support any actual multiprocess communication (e.g. sending
or accepting Mojo invitations), a second IPC initialization step is required.

## IPC Initialization

Internal Mojo IPC implementation requires background TaskRunner on which it can
watch for inbound I/O from other processes. This is configured using a
`ScopedIPCSupport` object, which keeps IPC support alive through the extent of
its lifetime.

Typically an application will create a dedicated background thread and give its
TaskRunner to Mojo. Note that in Chromium, we use the existing "IO thread" in
the browser process and content child processes.

```
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"

int main(int argc, char** argv) {
  mojo::edk::Init();

  base::Thread ipc_thread("ipc!");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  // As long as this object is alive, all Mojo API surface relevant to IPC
  // connections is usable, and message pipes which span a process boundary will
  // continue to function.
  mojo::edk::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return 0;
}
```

This process is now fully prepared to use Mojo IPC!

Note that all existing process types in Chromium already perform this setup
very early during startup.

## Connecting Two Processes

Once IPC is initialized, you can bootstrap connections to other processes by
using the public
[Invitations API](/mojo/public/cpp/system/README.md#Invitations).

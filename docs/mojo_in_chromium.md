# Mojo in Chromium

**THIS DOCUIMENT IS A WORK IN PROGRESS.** As long as this notice exists, you
should probably ignore everything below it.

This document is intended to serve as a Mojo primer for Chromium developers. No
prior knowledge of Mojo is assumed, but you should have a decent grasp of C++
and be familiar with Chromium's multi-process architecture as well as common
concepts used throughout Chromium such as smart pointers, message loops,
callback binding, and so on.

[TOC]

## Should I Bother Reading This?

If you're planning to build a Chromium feature that needs IPC and you aren't
already using Mojo, you probably want to read this. **Legacy IPC** -- _i.e._,
`foo_messages.h` files, message filters, and the suite of `IPC_MESSAGE_*` macros
-- **is on the verge of deprecation.**

## Why Mojo?

Mojo provides IPC primitives for pushing messages and data around between
transferrable endpoints which may or may not cross process boundaries; it
simplifies threading with regard to IPC; it standardizes message serialization
in a way that's resilient to versioning issues; and it can be used with relative
ease and consistency across a number of languages including C++, Java, and
`JavaScript` -- all languages which comprise a significant share of Chromium
code.

The messaging protocol doesn't strictly need to be used for IPC though, and
there are some higher-level reasons for this adoption and for the specific
approach to integration outlined in this document.

### Code Health

At the moment we have fairly weak separation between components, with DEPS being
the strongest line of defense against increasing complexity.

A component Foo might hold a reference to some bit of component Bar's internal
state, or it might expect Bar to initialize said internal state in some
particular order. These sorts of problems are reasonably well-mitigated by the
code review process, but they can (and do) still slip through the cracks, and
they have a noticeable cumulative effect on complexity as the code base
continues to grow.

We think we can make a lasting positive impact on code health by establishing
more concrete boundaries between components, and this is something a library
like Mojo gives us an opportunity to do.

### Modularity

In addition to code health -- which alone could be addressed in any number of
ways that don't involve Mojo -- this approach opens doors to build and
distribute parts of Chrome separately from the main binary.

While we're not currently taking advantage of this capability, doing so remains
a long-term goal due to prohibitive binary size constraints in emerging mobile
markets. Many open questions around the feasibility of this goal should be
answered by the experimental Mandoline project as it unfolds, but the Chromium
project can be technically prepared for such a transition in the meantime.

### Mandoline

The Mandoline project is producing a potential replacement for `src/content`.
Because Mandoline components are Mojo apps, and Chromium is now capable of
loading Mojo apps (somethings we'll discuss later), Mojo apps can be shared
between both projects with minimal effort. Developing your feature as or within
a Mojo application can mean you're contributing to both Chromium and Mandoline.

## Mojo Overview

This section provides a general overview of Mojo and some of its API features.
You can probably skip straight to
[Your First Mojo Application](#Your-First-Mojo-Application) if you just want to
get to some practical sample code.

The Mojo Embedder Development Kit (EDK) provides a suite of low-level IPC
primitives: **message pipes**, **data pipes**, and **shared buffers**. We'll
focus primarily on message pipes and the C++ bindings API in this document.

_TODO: Java and JS bindings APIs should also be covered here._

### Message Pipes

A message pipe is a lightweight primitive for reliable, bidirectional, queued
transfer of relatively small packets of data. Every pipe endpoint  is identified
by a **handle** -- a unique process-wide integer identifying the endpoint to the
EDK.

A single message across a pipe consists of a binary payload and an array of zero
or more handles to be transferred. A pipe's endpoints may live in the same
process or in two different processes.

Pipes are easy to create. The `mojo::MessagePipe` type (see
`/third_party/mojo/src/mojo/public/cpp/system/message_pipe.h`) provides a nice
class wrapper with each endpoint represented as a scoped handle type (see
members `handle0` and `handle1` and the definition of
`mojo::ScopedMessagePipeHandle`). In the same header you can find
`WriteMessageRaw` and `ReadMessageRaw` definitions. These are in theory all one
needs to begin pushing things from one endpoint to the other.

While it's worth being aware of `mojo::MessagePipe` and the associated raw I/O
functions, you will rarely if ever have a use for them. Instead you'll typically
use bindings code generated from mojom interface definitions, along with the
public bindings API which mostly hides the underlying pipes.

### Mojom Bindings

Mojom is the IDL for Mojo interfaces. When given a mojom file, the bindings
generator outputs a collection of bindings libraries for each supported
language. Mojom syntax is fairly straightforward (TODO: Link to a mojom language
spec?). Consider the example mojom file below:

```
// frobinator.mojom
module frob;
interface Frobinator {
  Frobinate();
};
```

This can be used to generate bindings for a very simple `Frobinator` interface.
Bindings are generated at build time and will match the location of the mojom
source file itself, mapped into the generated output directory for your Chromium
build. In this case one can expect to find files named `frobinator.mojom.js`,
`frobinator.mojom.cc`, `frobinator.mojom.h`, _etc._

The C++ header (`frobinator.mojom.h`) generated from this mojom will define a
pure virtual class interface named `frob::Frobinator` with a pure virtual method
of signature `void Frobinate()`. Any class which implements this interface is
effectively a `Frobinator` service.

### C++ Bindings API

Before we see an example implementation and usage of the Frobinator, there are a
handful of interesting bits in the public C++ bindings API you should be
familiar with. These complement generated bindings code and generally obviate
any need to use a `mojo::MessagePipe` directly.

In all of the cases below, `T` is the type of a generated bindings class
interface, such as the `frob::Frobinator` discussed above.

#### `mojo::InterfacePtr<T>`

Defined in `/third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr.h`.

`mojo::InterfacePtr<T>` is a typed proxy for a service of type `T`, which can be
bound to a message pipe endpoint. This class implements every interface method
on `T` by serializing a message (encoding the method call and its arguments) and
writing it to the pipe (if bound.) This is the standard way for C++ code to talk
to any Mojo service.

For illustrative purposes only, we can create a message pipe and bind an
`InterfacePtr` to one end as follows:

```cpp
  mojo::MessagePipe pipe;
  mojo::InterfacePtr<frob::Frobinator> frobinator;
  frobinator.Bind(
      mojo::InterfacePtrInfo<frob::Frobinator>(pipe.handle0.Pass(), 0u));
```

You could then call `frobinator->Frobinate()` and read the encoded `Frobinate`
message from the other side of the pipe (`handle1`.) You most likely don't want
to do this though, because as you'll soon see there's a nicer way to establish
service pipes.

#### `mojo::InterfaceRequest<T>`

Defined in `/third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h`.

`mojo::InterfaceRequest<T>` is a typed container for a message pipe endpoint
that should _eventually_ be bound to a service implementation. An
`InterfaceRequest` doesn't actually _do_ anything, it's just a way of holding
onto an endpoint without losing interface type information.

A common usage pattern is to create a pipe, bind one end to an
`InterfacePtr<T>`, and pass the other end off to someone else (say, over some
other message pipe) who is expected to eventually bind it to a concrete service
implementation. `InterfaceRequest<T>` is here for that purpose and is, as we'll
see later, a first-class concept in Mojom interface definitions.

As with `InterfacePtr<T>`, we can manually bind an `InterfaceRequest<T>` to a
pipe endpoint:

```cpp
mojo::MessagePipe pipe;

mojo::InterfacePtr<frob::Frobinator> frobinator;
frobinator.Bind(
    mojo::InterfacePtrInfo<frob::Frobinator>(pipe.handle0.Pass(), 0u));

mojo::InterfaceRequest<frob::Frobinator> frobinator_request;
frobinator_request.Bind(pipe.handle1.Pass());
```

At this point we could start making calls to `frobinator->Frobinate()` as
before, but they'll just sit in queue waiting for the request side to be bound.
Note that the basic logic in the snippet above is such a common pattern that
there's a convenient API function which does it for us.

#### `mojo::GetProxy<T>`

Defined in
`/third_party/mojo/src/mojo/public/cpp/bindings/interface`_request.h`.

`mojo::GetProxy<T>` is the function you will most commonly use to create a new
message pipe. Its signature is as follows:

```cpp
template <typename T>
mojo::InterfaceRequest<T> GetProxy(mojo::InterfacePtr<T>* ptr);
```

This function creates a new message pipe, binds one end to the given
`InterfacePtr` argument, and binds the other end to a new `InterfaceRequest`
which it then returns. Equivalent to the sample code just above is the following
snippet:

```cpp
  mojo::InterfacePtr<frob::Frobinator> frobinator;
  mojo::InterfaceRequest<frob::Frobinator> frobinator_request =
      mojo::GetProxy(&frobinator);
```

#### `mojo::Binding<T>`

Defined in `/third_party/mojo/src/mojo/public/cpp/bindings/binding.h`.

Binds one end of a message pipe to an implementation of service `T`. A message
sent from the other end of the pipe will be read and, if successfully decoded as
a `T` message, will invoke the corresponding call on the bound `T`
implementation. A `Binding<T>` must be constructed over an instance of `T`
(which itself usually owns said `Binding` object), and its bound pipe is usually
taken from a passed `InterfaceRequest<T>`.

A common usage pattern looks something like this:

```cpp
#include "components/frob/public/interfaces/frobinator.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

class FrobinatorImpl : public frob::Frobinator {
 public:
  FrobinatorImpl(mojo::InterfaceRequest<frob::Frobinator> request)
      : binding_(this, request.Pass()) {}
  ~FrobinatorImpl() override {}

 private:
  // frob::Frobinator:
  void Frobinate() override { /* ... */ }

  mojo::Binding<frob::Frobinator> binding_;
};
```

And then we could write some code to test this:

```cpp
// Fun fact: The bindings generator emits a type alias like this for every
// interface type. frob::FrobinatorPtr is an InterfacePtr<frob::Frobinator>.
frob::FrobinatorPtr frobinator;
scoped_ptr<FrobinatorImpl> impl(
    new FrobinatorImpl(mojo::GetProxy(&frobinator)));
frobinator->Frobinate();
```

This will _eventually_ call `FrobinatorImpl::Frobinate()`. "Eventually," because
the sequence of events when `frobinator->Frobinate()` is called is roughly as
follows:

1.  A new message buffer is allocated and filled with an encoded 'Frobinate'
    message.
1.  The EDK is asked to write this message to the pipe endpoint owned by the
    `FrobinatorPtr`.
1.  If the call didn't happen on the Mojo IPC thread for this process, EDK hops
    to the Mojo IPC thread.
1.  The EDK writes the message to the pipe. In this case the pipe endpoints live
    in the same process, so this essentially a glorified `memcpy`. If they lived
    in different processes this would be the point at which the data moved
    across a real IPC channel.
1.  The EDK on the other end of the pipe is awoken on the Mojo IPC thread and
    alerted to the message arrival.
1.  The EDK reads the message.
1.  If the bound receiver doesn't live on the Mojo IPC thread, the EDK hops to
    the receiver's thread.
1.  The message is passed on to the receiver. In this case the receiver is
    generated bindings code, via `Binding<T>`. This code decodes and validates
    the `Frobinate` message.
1.  `FrobinatorImpl::Frobinate()` is called on the bound implementation.

So as you can see, the call to `Frobinate()` may result in up to two thread hops
and one process hop before the service implementation is invoked.

#### `mojo::StrongBinding<T>`

Defined in `third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h`.

`mojo::StrongBinding<T>` is just like `mojo::Binding<T>` with the exception that
a `StrongBinding` takes ownership of the bound `T` instance. The instance is
destroyed whenever the bound message pipe is closed. This is convenient in cases
where you want a service implementation to live as long as the pipe it's
servicing, but like all features with clever lifetime semantics, it should be
used with caution.

## The Mojo Shell

Both Chromium and Mandoline run a central **shell** component which is used to
coordinate communication among all Mojo applications (see the next section for
an overview of Mojo applications.)

Every application receives a proxy to this shell upon initialization, and it is
exclusively through this proxy that an application can request connections to
other applications. The `mojo::Shell` interface provided by this proxy is
defined as follows:

```
module mojo;
interface Shell {
  ConnectToApplication(URLRequest application_url,
                       ServiceProvider&? services,
                       ServiceProvider? exposed_services);
  QuitApplication();
};
```

and as for the `mojo::ServiceProvider` interface:

```
module mojo;
interface ServiceProvider {
  ConnectToService(string interface_name, handle<message_pipe> pipe);
};
```

Definitions for these interfaces can be found in
`/mojo/shell/public/interfaces`. Also note that `mojo::URLRequest` is a
Mojo struct defined in
`/mojo/services/network/public/interfaces/url_loader.mojom`.

Note that there's some new syntax in the mojom for `ConnectToApplication` above.
The '?' signifies a nullable value and the '&' signifies an interface request
rather than an interface proxy.

The argument `ServiceProvider&? services` indicates that the caller should pass
an `InterfaceRequest<ServiceProvider>` as the second argument, but that it need
not be bound to a pipe (i.e., it can be "null" in which case it's ignored.)

The argument `ServiceProvider? exposed_services` indicates that the caller
should pass an `InterfacePtr<ServiceProvider>` as the third argument, but that
it may also be null.

`ConnectToApplication` asks the shell to establish a connection between the
caller and some other app the shell might know about. In the event that a
connection can be established -- which may involve the shell starting a new
instance of the target app -- the given `services` request (if not null) will be
bound to a service provider in the target app. The target app may in turn use
the passed `exposed_services` proxy (if not null) to request services from the
connecting app.

### Mojo Applications

All code which runs in a Mojo environment, apart from the shell itself (see
above), belongs to one Mojo **application** or another**`**`**. The term
"application" in this context is a common source of confusion, but it's really a
simple concept. In essence an application is anything which implements the
following Mojom interface:

```
module mojo;
interface Application {
  Initialize(Shell shell, string url);
  AcceptConnection(string requestor_url,
                   ServiceProvider&? services,
                   ServiceProvider? exposed_services,
                   string resolved_url);
  OnQuitRequested() => (bool can_quit);
};
```

Of course, in Chromium and Mandoline environments this interface is obscured
from application code and applications should generally just implement
`mojo::ApplicationDelegate` (defined in
`/mojo/shell/public/cpp/application_delegate.h`.) We'll see a concrete
example of this in the next section,
[Your First Mojo Application](#Your-First-Mojo-Application).

The takeaway here is that an application can be anything. It's not necessarily a
new process (though at the moment, it's at least a new thread). Applications can
connect to each other, and these connections are the mechanism through which
separate components expose services to each other.

**NOTE##: This is not true in Chromium today, but it should be eventually. For
some components (like render frames, or arbitrary browser process code) we
provide APIs which allow non-Mojo-app-code to masquerade as a Mojo app and
therefore connect to real Mojo apps through the shell.

### Other IPC Primitives

Finally, it's worth making brief mention of the other types of IPC primitives
Mojo provides apart from message pipes. A **data pipe** is a unidirectional
channel for pushing around raw data in bulk, and a **shared buffer** is
(unsurprisingly) a shared memory primitive. Both of these objects use the same
type of transferable handle as message pipe endpoints, and can therefore be
transferred across message pipes, potentially to other processes.

## Your First Mojo Application

In this section, we're going to build a simple Mojo application that can be run
in isolation using Mandoline's `mojo_runner` binary. After that we'll add a
service to the app and set up a test suite to connect and test that service.

### Hello, world!

So, you're building a new Mojo app and it has to live somewhere. For the
foreseeable future we'll likely be treating `//components` as a sort of
top-level home for new Mojo apps in the Chromium tree. Any component application
you build should probably go there. Let's create some basic files to kick things
off. You may want to start a new local Git branch to isolate any changes you
make while working through this.

First create a new `//components/hello` directory. Inside this directory we're
going to add the following files:

**components/hello/main.cc**

```cpp
#include "base/logging.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  LOG(ERROR) << "Hello, world!";
  return MOJO_RESULT_OK;
};
```

**components/hello/BUILD.gn**

```
import("//mojo/public/mojo_application.gni")

mojo_native_application("hello") {
  sources = [
    "main.cc",
  ]
  deps = [
    "//base",
    "//mojo/environment:chromium",
  ]
}
```

For the sake of this example you'll also want to add your component as a
dependency somewhere in your local checkout to ensure its build files are
generated. The easiest thing to do there is probably to add a dependency on
`"//components/hello"` in the `"gn_all"` target of the top-level `//BUILD.gn`.

Assuming you have a GN output directory at `out_gn/Debug`, you can build the
Mojo runner along with your shiny new app:

    ninja -C out_gn/Debug mojo_runner components/hello

In addition to the `mojo_runner` executable, this will produce a new binary at
`out_gn/Debug/hello/hello.mojo`. This binary is essentially a shared library
which exports your `MojoMain` function.

`mojo_runner` takes an application URL as its only argument and runs the
corresponding application. In its current state it resolves `mojo`-scheme URLs
such that `"mojo:foo"` maps to the file `"foo/foo.mojo"` relative to the
`mojo_runner` path (_i.e._ your output directory.) This means you can run your
new app with the following command:

    out_gn/Debug/mojo_runner mojo:hello

You should see our little `"Hello, world!"` error log followed by a hanging
application. You can `^C` to kill it.

### Exposing Services

An app that prints `"Hello, world!"` isn't terribly interesting. At a bare
minimum your app should implement `mojo::ApplicationDelegate` and expose at
least one service to connecting applications.

Let's update `main.cc` with the following contents:

**components/hello/main.cc**

```cpp
#include "components/hello/hello_app.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new hello::HelloApp);
  return runner.Run(shell_handle);
};
```

This is a pretty typical looking `MojoMain`. Most of the time this is all you
want -- a `mojo::ApplicationRunner` constructed over a
`mojo::ApplicationDelegate` instance, `Run()` with the pipe handle received from
the shell. We'll add some new files to the app as well:

**components/hello/public/interfaces/greeter.mojom**

```
module hello;
interface Greeter {
  Greet(string name) => (string greeting);
};
```

Note the new arrow syntax on the `Greet` method. This indicates that the caller
expects a response from the service.

**components/hello/public/interfaces/BUILD.gn**

```
import("//third_party/mojo/src/mojo/public/tools/bindings/mojom.gni")

mojom("interfaces") {
  sources = [
    "greeter.mojom",
  ]
}
```

**components/hello/hello_app.h**

```cpp
#ifndef COMPONENTS_HELLO_HELLO_APP_H_
#define COMPONENTS_HELLO_HELLO_APP_H_

#include "base/macros.h"
#include "components/hello/public/interfaces/greeter.mojom.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace hello {

class HelloApp : public mojo::ApplicationDelegate,
                 public mojo::InterfaceFactory<Greeter> {
 public:
  HelloApp();
  ~HelloApp() override;

 private:
  // mojo::ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mojo::InterfaceFactory<Greeter>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<Greeter> request) override;

  DISALLOW_COPY_AND_ASSIGN(HelloApp);
};

}  // namespace hello

#endif  // COMPONENTS_HELLO_HELLO_APP_H_
```


**components/hello/hello_app.cc**

```cpp
#include "base/macros.h"
#include "components/hello/hello_app.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace hello {

namespace {

class GreeterImpl : public Greeter {
 public:
  GreeterImpl(mojo::InterfaceRequest<Greeter> request)
      : binding_(this, request.Pass()) {
  }

  ~GreeterImpl() override {}

 private:
  // Greeter:
  void Greet(const mojo::String& name, const GreetCallback& callback) override {
    callback.Run("Hello, " + std::string(name) + "!");
  }

  mojo::StrongBinding<Greeter> binding_;

  DISALLOW_COPY_AND_ASSIGN(GreeterImpl);
};

}  // namespace

HelloApp::HelloApp() {
}

HelloApp::~HelloApp() {
}

bool HelloApp::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<Greeter>(this);
  return true;
}

void HelloApp::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<Greeter> request) {
  new GreeterImpl(request.Pass());
}

}  // namespace hello
```

And finally we need to update our app's `BUILD.gn` to add some new sources and
dependencies:

**components/hello/BUILD.gn**

```
import("//mojo/public/mojo_application.gni")

source_set("lib") {
  sources = [
    "hello_app.cc",
    "hello_app.h",
  ]
  deps = [
    "//base",
    "//components/hello/public/interfaces",
    "//mojo/environment:chromium",
    "//mojo/shell/public/cpp",
  ]
}

mojo_native_application("hello") {
  sources = [
    "main.cc",
  ],
  deps = [ ":lib" ]
}
```

Note that we build the bulk of our application sources as a static library
separate from the `MojoMain` definition. Following this convention is
particularly useful for Chromium integration, as we'll see later.

There's a lot going on here and it would be useful to familiarize yourself with
the definitions of `mojo::ApplicationDelegate`, `mojo::ApplicationConnection`,
and `mojo::InterfaceFactory<T>`. The TL;DR though is that if someone connects to
this app and requests a service named `"hello::Greeter"`, the app will create a
new `GreeterImpl` and bind it to that request pipe. From there the connecting
app can call `Greeter` interface methods and they'll be routed to that
`GreeterImpl` instance.

Although this appears to be a more interesting application, we need some way to
actually connect and test the behavior of our new service. Let's write an app
test!

### App Tests

App tests run inside a test application, giving test code access to a shell
which can connect to one or more applications-under-test.

First let's introduce some test code:

**components/hello/hello_apptest.cc**

```cpp
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/hello/public/interfaces/greeter.mojom.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"

namespace hello {
namespace {

class HelloAppTest : public mojo::test::ApplicationTestBase {
 public:
  HelloAppTest() {}
  ~HelloAppTest() override {}

  void SetUp() override {
    ApplicationTestBase::SetUp();
    mojo::URLRequestPtr app_url = mojo::URLRequest::New();
    app_url->url = "mojo:hello";
    application_impl()->ConnectToService(app_url.Pass(), &greeter_);
  }

  Greeter* greeter() { return greeter_.get(); }

 private:
  GreeterPtr greeter_;

  DISALLOW_COPY_AND_ASSIGN(HelloAppTest);
};

void ExpectGreeting(const mojo::String& expected_greeting,
                    const base::Closure& continuation,
                    const mojo::String& actual_greeting) {
  EXPECT_EQ(expected_greeting, actual_greeting);
  continuation.Run();
};

TEST_F(HelloAppTest, GreetWorld) {
  base::RunLoop loop;
  greeter()->Greet("world", base::Bind(&ExpectGreeting, "Hello, world!",
                                       loop.QuitClosure()));
  loop.Run();
}

}  // namespace
}  // namespace hello
```

We also need to add a new rule to `//components/hello/BUILD.gn`:

```
mojo_native_application("apptests") {
  output_name = "hello_apptests"
  testonly = true
  sources = [
    "hello_apptest.cc",
  ]
  deps = [
    "//base",
    "//mojo/shell/public/cpp:test_support",
  ]
  public_deps = [
    "//components/hello/public/interfaces",
  ]
  data_deps = [ ":hello" ]
}
```

Note that the `//components/hello:apptests` target does **not** have a binary
dependency on either `HelloApp` or `GreeterImpl` implementations; instead it
depends only on the component's public interface definitions.

The `data_deps` entry ensures that `hello.mojo` is up-to-date when `apptests` is
built. This is desirable because the test connects to `"mojo:hello"` which will
in turn load `hello.mojo` from disk.

You can now build the test suite:

    ninja -C out_gn/Debug components/hello:apptests

and run it:

    out_gn/Debug/mojo_runner mojo:hello_apptests

You should see one test (`HelloAppTest.GreetWorld`) passing.

One particularly interesting bit of code in this test is in the `SetUp` method:

    mojo::URLRequestPtr app_url = mojo::URLRequest::New();
    app_url->url = "mojo:hello";
    application_impl()->ConnectToService(app_url.Pass(), &greeter_);

`ConnectToService` is a convenience method provided by `mojo::ApplicationImpl`,
and it's essentially a shortcut for calling out to the shell's
`ConnectToApplication` method with the given application URL (in this case
`"mojo:hello"`) and then connecting to a specific service provided by that app
via its `ServiceProvider`'s `ConnectToService` method.

Note that generated interface bindings include a constant string to identify
each interface by name; so for example the generated `hello::Greeter` type
defines a static C string:

    const char hello::Greeter::Name_[] = "hello::Greeter";

This is exploited by the definition of
`mojo::ApplicationConnection::ConnectToService<T>`, which uses `T::Name_` as the
name of the service to connect to. The type `T` in this context is inferred from
the `InterfacePtr<T>*` argument. You can inspect the definition of
`ConnectToService` in `/mojo/shell/public/cpp/application_connection.h`
for additional clarity.

We could have instead written this code as:

```cpp
mojo::URLRequestPtr app_url = mojo::URLRequest::New();
app_url->url = "mojo::hello";

mojo::ServiceProviderPtr services;
application_impl()->shell()->ConnectToApplication(
    app_url.Pass(), mojo::GetProxy(&services),
    // We pass a null provider since we aren't exposing any of our own
    // services to the target app.
    mojo::ServiceProviderPtr());

mojo::InterfaceRequest<hello::Greeter> greeter_request =
    mojo::GetProxy(&greeter_);
services->ConnectToService(hello::Greeter::Name_,
                           greeter_request.PassMessagePipe());
```

The net result is the same, but 3-line version seems much nicer.

## Chromium Integration

Up until now we've been using `mojo_runner` to load and run `.mojo` binaries
dynamically. While this model is used by Mandoline and may eventually be used in
Chromium as well, Chromium is at the moment confined to running statically
linked application code. This means we need some way to register applications
with the browser's Mojo shell.

It also means that, rather than using the binary output of a
`mojo_native_application` target, some part of Chromium must link against the
app's static library target (_e.g._, `"//components/hello:lib"`) and register a
URL handler to teach the shell how to launch an instance of the app.

When registering an app URL in Chromium it probably makes sense to use the same
mojo-scheme URL used for the app in Mandoline. For example the media renderer
app is referenced by the `"mojo:media"` URL in both Mandoline and Chromium. In
Mandoline this resolves to a dynamically-loaded `.mojo` binary on disk, but in
Chromium it resolves to a static application loader linked into Chromium. The
net result is the same in both cases: other apps can use the shell to connect to
`"mojo:media"` and use its services.

This section explores different ways to register and connect to `"mojo:hello"`
in Chromium.

### In-Process Applications

Applications can be set up to run within the browser process via
`ContentBrowserClient::RegisterInProcessMojoApplications`. This method populates
a mapping from URL to `base::Callback<scoped_ptr<mojo::ApplicationDelegate>()>`
(_i.e._, a factory function which creates a new `mojo::ApplicationDelegate`
instance), so registering a new app means adding an entry to this map.

Let's modify `ChromeContentBrowserClient::RegisterInProcessMojoApplications`
(in `//chrome/browser/chrome_content_browser_client.cc`) by adding the following
code:

```cpp
apps->insert(std::make_pair(GURL("mojo:hello"),
                            base::Bind(&HelloApp::CreateApp)));
```

you'll also want to add the following convenience method to your `HelloApp`
definition in `//components/hello/hello_app.h`:

```cpp
static scoped_ptr<mojo::ApplicationDelegate> HelloApp::CreateApp() {
  return scoped_ptr<mojo::ApplicationDelegate>(new HelloApp);
}
```

This introduces a dependency from `//chrome/browser` on to
`//components/hello:lib`, which you can add to the `"browser"` target's deps in
`//chrome/browser/BUILD.gn`. You'll of course also need to include
`"components/hello/hello_app.h"` in `chrome_content_browser_client.cc`.

That's it! Now if an app comes to the shell asking to connect to `"mojo:hello"`
and app is already running, it'll get connected to our `HelloApp` and have
access to the `Greeter` service. If the app wasn't already running, it will
first be launched on a new thread.

### Connecting From the Browser

We've already seen how apps can connect to each other using their own private
shell proxy, but the vast majority of Chromium code doesn't yet belong to a Mojo
application. So how do we use an app's services from arbitrary browser code? We
use `content::MojoAppConnection`, like this:

```cpp
#include "base/bind.h"
#include "base/logging.h"
#include "components/hello/public/interfaces/greeter.mojom.h"
#include "content/public/browser/mojo_app_connection.h"

void LogGreeting(const mojo::String& greeting) {
  LOG(INFO) << greeting;
}

void GreetTheWorld() {
  scoped_ptr<content::MojoAppConnection> connection =
      content::MojoAppConnection::Create("mojo:hello",
                                         content::kBrowserMojoAppUrl);
  hello::GreeterPtr greeter;
  connection->ConnectToService(&greeter);
  greeter->Greet("world", base::Bind(&LogGreeting));
}
```

A `content::MojoAppConnection`, while not thread-safe, may be created and safely
used on any single browser thread.

You could add the above code to a new browsertest to convince yourself that it
works. In fact you might want to take a peek at
`MojoShellTest.TestBrowserConnection` (in
`/content/browser/mojo_shell_browsertest.cc`) which registers and tests an
in-process Mojo app.

Finally, note that `MojoAppConnection::Create` takes two URLs. The first is the
target app URL, and the second is the source URL. Since we're not really a Mojo
app, but we are still trusted browser code, the shell will gladly use this URL
as the `requestor_url` when establishing an incoming connection to the target
app. This allows browser code to masquerade as a Mojo app at the given URL.
`content::kBrowserMojoAppUrl` (which is presently `"system:content_browser"`) is
a reasonable default choice when a more specific app identity isn't required.

### Out-of-Process Applications

If an app URL isn't registered for in-process loading, the shell assumes it must
be an out-of-process application. If the shell doesn't already have a known
instance of the app running, a new utility process is launched and the
application request is passed onto it. Then if the app URL is registered in the
utility process, the app will be loaded there.

Similar to in-process registration, a URL mapping needs to be registered in
`ContentUtilityClient::RegisterMojoApplications`.

Once again you can take a peek at `/content/browser/mojo_shell_browsertest.cc`
for an end-to-end example of testing an out-of-process Mojo app from browser
code. Note that `content_browsertests` runs on `content_shell`, which uses
`ShellContentUtilityClient` as defined
`/content/shell/utility/shell_content_utility_client.cc`. This code registers a
common OOP test app.

## Unsandboxed Out-of-Process Applications

By default new utility processes run in a sandbox. If you want your Mojo app to
run out-of-process and unsandboxed (which you **probably do not**), you can
register its URL via
`ContentBrowserClient::RegisterUnsandboxedOutOfProcessMojoApplications`.

## Connecting From `RenderFrame`

We can also connect to Mojo apps from a `RenderFrame`. This is made possible by
`RenderFrame`'s `GetServiceRegistry()` interface. The `ServiceRegistry` can be
used to acquire a shell proxy and in turn connect to an app like so:

```cpp
void GreetWorld(content::RenderFrame* frame) {
  mojo::ShellPtr shell;
  frame->GetServiceRegistry()->ConnectToRemoteService(
      mojo::GetProxy(&shell));

  mojo::URLRequestPtr request = mojo::URLRequest::New();
  request->url = "mojo:hello";

  mojo::ServiceProviderPtr hello_services;
  shell->ConnectToApplication(
      request.Pass(), mojo::GetProxy(&hello_services), nullptr);

  hello::GreeterPtr greeter;
  hello_services->ConnectToService(
      hello::Greeter::Name_, mojo::GetProxy(&greeter).PassMessagePipe());
}
```

It's important to note that connections made through the frame's shell proxy
will appear to come from the frame's `SiteInstance` URL. For example, if the
frame has loaded `https://example.com/`, `HelloApp`'s incoming
`mojo::ApplicationConnection` in this case will have a remote application URL of
`"https://example.com/"`. This allows apps to expose their services to web
frames on a per-origin basis if needed.

### Connecting From Java

TODO

### Connecting From `JavaScript`

This is still a work in progress and might not really take shape until the
Blink+Chromium merge. In the meantime there are some end-to-end WebUI examples
in `/content/browser/webui/web_ui_mojo_browsertest.cc`. In particular,
`WebUIMojoTest.ConnectToApplication` connects from a WebUI frame to a test app
running in a new utility process.

## FAQ

Nothing here yet!

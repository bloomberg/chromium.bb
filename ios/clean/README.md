# iOS Chrome Architecture

`/ios/clean/` houses a revised and updated architecture for iOS Chrome's UI. iOS
is in the process of migrating into this new architecture. During this process,
code in `/ios/clean` should not depend on code in `ios/chrome/browser/ui`, or
vice-versa; instead code that is shared between the two will be moved into
`ios/shared`.

This document is a general overview of the fundamental concepts of the new
architecture. More specific documentation is located alongside the
implementations.

[TOC]

## Goals and core principles.

The goals of the new architecture, briefly, are to:

* Allow for radical user interface experimentation and changes.
* Make development and maintenance of features easier.
* Cut the Gordian knots of the existing architecture, removing the need for
  [unmaintainable monster classes](/ios/chrome/browser/ui/BrowserViewController.mm)
  that accrete functionality.
* Keep up with the evolution if iOS by providing a structure in which the
  affordances provided by Cocoa Touch can be used without radical changes.

In order to meet these goals, the new architecture is designed according to
these three core principles:

* **Strong Decoupling.** Components of the browser, especially UI components,
  should not depend on other components explicitly. Where components interact,
  they should do so through protocols or other abstractions that prevent
  tight coupling.
* **Strong Encapsulation.** A complement to decoupling is encapsulation.
  Components of the browser should expose little specifics about their
  implementation or state. Public APIs should be as small as possible.
  Architectural commonalities (for example, the use of a common superclass for
  coordinators) will mean that the essential interfaces for complex components
  can be both small and common across many implementations. Overall the
  combination of decoupling and encapsulation means that components of
  Chrome can be rearranged or removed without impacting the rest of the
  application.
* **Separation of layers.** Objects should operate at a specific layer, and
  their interactions with objects in other layers should be well-defined. We
  distinguish model, coordinator, and UI layers.

## Structure.

### View controllers.

Since this is an iOS application, view controllers remain a fundamental building
block of the user interface. However, the scope of a view controller’s
responsibilities are strongly constrained. View controllers are responsible for
the display and user interactions of a specific piece of user interface. They
may contain, or be contained in, other view controllers in order to compose the
complete UI that a user interacts with.

For example, when viewing a web page in a Chrome tab, the web page contents, the
toolbar above the page, and the location bar in the toolbar are each separate
view controllers, and a fourth view controller is responsible for composing them
together. View controllers do not directly interact with each other or with any
model-layer services.

### Coordinators.

Since view controllers are heavily encapsulated, another object is responsible
for creating them and connecting them to other objects. This object is a
coordinator, and in this architecture all coordinators are subclasses of
[BrowserCoordinator](/ios/chrome/browser/ui/coordinators/). Coordinators
exist in a hierarchy, roughly parallel to the view controller hierarchy; each
coordinator can have multiple child coordinators, and there is a single root
coordinator that is created when the application launches.

The primary role of coordinators is to create and configure their view
controller, connect it to a mediator and/or a dispatcher (see below), and to
handle the creation and lifecycle of any child view controllers. Coordinators
shouldn’t directly interact with model-layer services, and should contain
little if any “business logic”.

### Mediators.

Mediators encapsulate the interactions between the model layer and the user
interface. They are created by coordinators and are handed connections to any
model-layer services they need, as well as to a “consumer” which they will
push user interface configuration and updates into.

The consumer is typically a view controller, and there is a consumer protocol
defined as part of the UI layer that the view controller adopts for this
purpose. A view controller that integrates UI updates from multiple model
services may be consuming from multiple mediators and may thus implement
multiple consumer protocols.

Mediators can be either Objective-C or C++ objects, and there isn't a universal
superclass for them.

### Service Objects.

Coordinators are created with access to a number of “service objects” that they
may connect to mediators and view controllers. By default a coordinator passes
pointers to all of these objects into their children.

The inventory of service objects includes:

* **[WebStateList:](/ios/chrome/browser/web_state_list/)** An observable,
  mutable, ordered collection of [WebState](/ios/public/web_state/)s. A WebState
  is the model-layer representation of a browser tab.
  
* **BrowserState:** The [ChromeBrowserState](/ios/chrome/browser/chrome_browser/state/)
  object as used in the old architecture.
  
* **[Dispatcher:](/ios/chrome/browser/ui/commands/)** A decoupled
  interface over which application-level method calls may be made; see below.
  
* **[Broadcaster:](/ios/shared/chrome/browser/ui/broadcaster)** A decoupled
  interface over which some properties of the user interface may be observed.

Some of these service objects are contained inside a [Browser](/ios/shared/chrome/browser/ui/browser_list)
object, which represents a collection of user tabs, equivalent to a window on
desktop platforms.

The Dispatcher merits special discussion. The role of the dispatcher is to
opaquely route Objective-C method calls. The dispatcher can register objects to
handle specific method calls, or an entire protocol. These protocols are
conventionally called [‘commands’](/ios/clean/chrome/browser/ui/commands/).

Coordinators typically register themselves or the mediators they own to handle
commands related to the area of their responsibility. The dispatcher is handed
into the view controller that a coordinator creates in the guise of a generic
Objective-C object that conforms to the command protocols that the view
controller needs. This allows the view controller to call methods on objects
that its coordinator doesn’t directly know about.

## Strictures.

Within this basic structure, and given the overall principles of decoupling,
encapsulation, and separation of layers, there are a number of restrictions that
all of these classes of objects should adhere to.

### View Controllers

* Cannot directly create or interact with any other specific classes of view
  controllers. View controllers that contain other view controllers should have
  public properties or methods for these embedded view controllers to be
  provided; internally they are limited to adding, removing, positioning, and
  resizing any such view controllers.

* Cannot depend on any model-layer objects or types.

* Should only operate on the main thread.

* Should not be their own transitioning delegates; an object from the
  coordinator layer (usually the coordinator) should handle that.

* Should handle action methods for their own UI, or, if appropriate, have the
  command-handling object they are given (a pointer to the dispatcher) be the
  target for control actions.
 
* Should only take input from other parts of the application by having them
  pushed over the consumer interface (or interfaces).

* Should only send messages to the rest of the application directly via the
  dispatcher.

### The UI layer

View controllers, the consumer protocols they adopt, and associated classes and
assets constitute the “UI layer” of the application, and it is a separate source
set in the gn configurations. The UI layer should have no dependencies on
coordinators, mediators, or model objects.

### Coordinators

* Should only pass a minimal set of service objects into mediators or other
  objects that require them. Rather than passing a `Browser` or `BrowserState`,
  they should pass specific services or sub-objects. Rather than passing the
  Dispatcher, they should cast it into an `i`d object that conforms to the
  protocol(s) the object requires.
 
* Should only handle command method whose execution is mostly a matter of
  creating, starting, or stopping other coordinators. Interactions with
  model-layer objects should be handled by mediators. Coordinators can have
  methods dispatch to their mediator.

* Should only operate on the main thread.

* If custom view controller presentation or animation is required, the
  coordinator can be the presented view controller’s transitioning delegate, or
  it can create a helper object for that.

### Mediators

* Should have any services they need passed into them by the coordinator. They
  shouldn’t depend on singletons, or on aggregate service objects such as
  `BrowserState`.

* Can communicate with their consumer _only_ over the consumer interface and
  _only_ on the main thread. It is the mediator’s responsibility to manage any
  model updates that occur on other threads and consolidate them into updates
  sent to the consumer on the main thread.

* Can only send Foundation value types, collections, or data classes containing
  them to their consumer.
 
* Cannot expose any kind of delegate interface for the consumer to call.
  Mediators push updates to consumers, and view controllers call command methods
  to induce model changes.
  
### Dispatchers

* Should only be used for "command-ish" methods. The dispatcher shouldn’t be
  used to implement what are functionally delegation, observation, or
  notification relationships. Other tools (such as the Broadcaster, and
  model-layer observers) exist for those purposes.


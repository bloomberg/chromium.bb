# Actions.

-----
**The files in this directory are only used in the new iOS Chrome
architecture.**
-----

This directory contains header files that define action protocols. Functionally
these protocols are used to add application-specific methods to `UIResponder`,
to allow controls to send action messages up the responder chain.

These protocols headers are in this directory because, although they relate to
specific UI features (settings, toolbar, etc), they will be used by different
UI components, and should be independent of any specific UI implementation.

Classes that instantiate controls that *send* the methods in one of these
protocols will need to import the header for that protocol, but won't need to
conform to it.

Classes in the responder chain that *handle* any of these methods need to
conform to the protocol.

These protocols are specifically for use in the UI-layer target/action context,
and shouldn't be used for other purposes. The `sender` parameter of any of these
methods should always be an actual `UIControl` instance that the user is
interacting with.

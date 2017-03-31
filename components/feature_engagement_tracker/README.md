# Feature Engagement Tracker

The Feature Engagement Tracker provides a client-side backend for displaying
feature enlightenment or in-product help (IPH) with a clean and easy to use API
to be consumed by the UI frontend. The backend behaves as a black box and takes
input about user behavior. Whenever the frontend gives a trigger signal that
in-product help could be displayed, the backend will provide an answer to
whether it is appropriate to show it or not.

The frontend only needs to deal with user interactions and how to display the
feature enlightenment or in-product help itself.

The backend is feature agnostic and have no special logic for any specific
features, but instead provides a generic API.

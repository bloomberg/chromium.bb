// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.Pair;

/**
 * Base class for mojo generated interfaces that have a client.
 *
 * @param <CI> the type of the client interface.
 */
public interface InterfaceWithClient<CI extends Interface> extends Interface {

    /**
     * Proxy class for interfaces with a client.
     */
    interface Proxy<CI extends Interface> extends Interface.Proxy, InterfaceWithClient<CI> {
    }

    /**
     * Base implementation of Proxy.
     *
     * @param <CI> the type of the client interface.
     */
    abstract class AbstractProxy<CI extends Interface> extends Interface.AbstractProxy
            implements Proxy<CI> {

        /**
         * Constructor.
         *
         * @param core the Core implementation used to create pipes and access the async waiter.
         * @param messageReceiver the message receiver to send message to.
         */
        public AbstractProxy(Core core, MessageReceiverWithResponder messageReceiver) {
            super(core, messageReceiver);
        }

        /**
         * @see InterfaceWithClient#setClient(Interface)
         */
        @Override
        public void setClient(CI client) {
            throw new UnsupportedOperationException(
                    "Setting the client on a proxy is not supported");
        }
    }

    /**
     * Base manager implementation for interfaces that have a client.
     *
     * @param <I> the type of the interface the manager can handle.
     * @param <P> the type of the proxy the manager can handle. To be noted, P always extends I.
     * @param <CI> the type of the client interface.
     */
    abstract class Manager<I extends InterfaceWithClient<CI>, P extends Proxy<CI>,
            CI extends Interface> extends Interface.Manager<I, P> {

        /**
         * @see Interface.Manager#bind(Interface, MessagePipeHandle)
         */
        @Override
        public final void bind(I impl, MessagePipeHandle handle) {
            Router router = new RouterImpl(handle);
            super.bind(handle.getCore(), impl, router);
            @SuppressWarnings("unchecked")
            CI client = (CI) getClientManager().attachProxy(handle.getCore(), router);
            impl.setClient(client);
            router.start();
        }

        /**
         * Returns a Proxy that will send messages to the given |handle|. This implies that the
         * other end of the handle must be connected to an implementation of the interface. |client|
         * is the implementation of the client interface.
         */
        public P attachProxy(MessagePipeHandle handle, CI client) {
            Router router = new RouterImpl(handle);
            DelegatingConnectionErrorHandler handlers = new DelegatingConnectionErrorHandler();
            handlers.addConnectionErrorHandler(client);
            router.setErrorHandler(handlers);
            getClientManager().bind(handle.getCore(), client, router);
            P proxy = super.attachProxy(handle.getCore(), router);
            handlers.addConnectionErrorHandler(proxy);
            router.start();
            return proxy;
        }

        /**
         * Constructs a new |InterfaceRequest| for the interface. This method returns a Pair where
         * the first element is a proxy, and the second element is the request. The proxy can be
         * used immediately.
         *
         * @param client the implementation of the client interface.
         */
        public final Pair<P, InterfaceRequest<I>> getInterfaceRequest(Core core, CI client) {
            Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
            P proxy = attachProxy(handles.first, client);
            return Pair.create(proxy, new InterfaceRequest<I>(handles.second));
        }

        /**
         * Returns a manager for the client inetrafce.
         */
        protected abstract Interface.Manager<CI, ?> getClientManager();
    }

    /**
     * Set the client implementation for this interface.
     */
    public void setClient(CI client);
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CEEE_IE_BROKER_EXECUTORS_MANAGER_DOCS_H_  // Mainly for lint
#define CEEE_IE_BROKER_EXECUTORS_MANAGER_DOCS_H_

/* @page ExecutorsManagerDoc Detailed documentation of the ExecutorsManager.

@section ExecutorsManagerIntro Introduction

The Executors Manager implemented by the ExecutorsManager class,
is used by the CeeeBroker object to instantiate and hold on to
Marshaled proxies of CeeeExecutor COM objects running in other processes.

The complexity lies in the fact that the Broker could send concurrent requests
to access Executor proxies and the creation of new proxies can't be done
synchronously.

So the Executors Manager must block on a request for a new Executor, and all
subsequent requests for the same executor until the asynchronous creation of
the Executor is complete. Once the Executor is completely registered in the
Executors Manager, it can be returned right away when requested (though a wait
may need to be done on a lock to access the map of Executors).

Executors are created to execute code in a given thread (called the
destination thread from now on), so we map the Executor to their associated
destination thread identifier and reuse existing Executors for subsequent
requests to execute in the same destination thread. We also keep an open
handle to the destination thread so that we can wait to it in a separate
Executors Manager @ref ThreadProc "worker thread" and wake up when the
destination thread dies, so that we can clean our map.

@section PublicMethods Public methods

The Executors Manager has has 3 public entry points, @ref GetExecutor,
@ref AddExecutor and @ref RemoveExecutor.

@subsection GetExecutor

The @link ExecutorsManager::GetExecutor GetExecutor @endlink method returns a
reference to a CeeeExecutor COM object (simply called Executor from now on)
for a given destination thread. It first checks for an existing Executor in
the @link ExecutorsManager::executors_ Executors map@endlink, and if it finds
one, it simply returns it (though it must @link ExecutorsManager::lock_ lock
@endlink the access to the Executors map).

If the @link ExecutorsManager::executors_ Executors map@endlink doesn't
already have an Executor for the requested
destination thread, we must first check if we already have a @link
ExecutorsManager::pending_registrations_ pending registration @endlink for that
same destination thread.

As mentioned @ref ExecutorsManagerIntro "above", registration requests can be
pending, since the creation of new Executors can not be done synchronously.

If there are no existing Executor and no pending registration to create one,
we can create a new pending registration (all of this, properly locked so that
we make sure to only start one pending registration for a given destination
thread).

The creation of new Executors is done via the usage of the @link
CeeeExecutorCreator Executor Creator @endlink COM object that is exposed by
a DLL so that it can inject a @link CeeeExecutorCreator::hook_ hook
@endlink in the destination thread to create the Executor. Once the hook is
injected, we can only post to the hook (since sending a message creates
a user-synchronous operation that can't get out of the process, so we won't be
able to register the newly created Executor in the Broker).

Once the Executor Creator posted a message to the hook to instantiate a new
Executor and get it registered in the Broker, the Executors Manager must wait
for the registration to complete. Since the Executors Manager is called by the
Broker, which is instantiated in a multi-threaded apartment, the registration
will happen in a free thread while the thread that made the call to GetExecutor
is blocked, waiting for the pending registration to complete.

At that point, there may be more than one thread calling GetExecutor for the
same destination thread, so they must all wait on the registration complete
event. Also, since there can be more than one destination thread with pending
registrations for a new Executor, there must be multiple events used to signal
the completion of the registration, one for each destination thread that has a
pending request to create a new executor. So the Executors Manager keeps a
@link ExecutorsManager::pending_registrations_ map of destination thread
identifier to registration completion event@endlink.

Once the registration of a new Executor is complete (via a call to @ref
RegisterTabExecutor), the event for the destination thread for which the
Executor is being registered is signaled so that any thread waiting for it can
wake up, get the new Executor from the map, and return it to their caller.

@subsection RegisterExecutor

The @link ExecutorsManager::RegisterExecutor RegisterExecutor @endlink method is
called by the Broker when it receives a request to register a new Executor for a
given destination thread. So the Executors Manager must first confirm that we
didn't already have an Executor for that destination thread, and also confirm
that we actually have a pending request for that destination thread.

@note <b>TODO(mad@chromium.org)</b>: We need to change this so that we
allow the BHO and maybe other modules in the future to preemptively
register Executors without a pending request.

Once this is confirmed, the new Executor is added to the map (again, in a
thread safe way, of course), and the event of the pending request to register
an executor for that destination thread is signaled. We also need to wake up
the @ref ThreadProc "worker thread", waiting on destination thread handles,
(using the @ref update_threads_list_gate_ event) so that the new one can be
added to the list to wait on.

@subsection RemoveExecutor

If for some reason, an executor must remove itself from the Executors Manager's
map, it can do so by calling the @link
ICeeeBrokerRegistrar::UnregisterExecutor UnregisterExecutor @endlink method
on the Broker ICeeeBrokerRegistrar interface which delegates to the
Executors Manager's @link ExecutorsManager::RemoveExecutor RemoveExecutor
@endlink method.

This same method is also called from the Executors Manager's @ref ThreadProc
"worker thread" waiting for the death of the destination thread for which we
have an executor in the map. When the thread dies, we remove the associated
executor from the map.

@subsection Terminate

The @link ExecutorsManager::Terminate Terminate @endlink method is to be called
when we are done with the Executors Manager, and it can clean up and release
all its resources (including stopping the @ref ThreadProc "worker thread" used
to wait for the destination threads to die, by setting the @ref
termination_gate_ event).

@section PrivateMethods Private methods

We use a few private methods to help with the implementation of the public
methods described @ref PublicMethods "above".

@subsection GetThreadHandles

@link ExecutorsManager::GetThreadHandles GetThreadHandles @endlink parses
through our @link ExecutorsManager::executors_ Executors map @endlink to return
the list of handles that the @ref ThreadProc "worker thread" has to wait for.

The caller must provide two pre-allocated arrays, one for thread handles and the
other one for thread identifiers. So we may not be able to return info for
all the threads if there isn't enough room. This is OK since the only caller is
the worker thread procedure (described @ref ThreadProc "below") and it can't
wait for more than a specific number of handles anyway. The arrays must be kept
in sync, so that index i in one of them refers to the same thread as index i in
the other.

@subsection ThreadProc

The @link ExecutorsManager::ThreadProc ThreadProc @endlink is the worker thread
procedure that is used to get the list of destination threads for
which we have an executor in the @link ExecutorsManager::executors_ Executors
map@endlink, and then wait on them to know when they die.

We also need to wait on two events, @ref termination_gate_ for the termination
of the worker thread (set in the Terminate public method described @ref
Terminate "above") and @ref update_threads_list_gate_ warning us that there are
new threads that we must wait on (set in the RegisterExecutor public method
described @ref RegisterExecutor "above").

@section PrivateDataMembers Private data members

@subsection executors_

@link ExecutorsManager::executors_ executors_ @endlink is a map from a
destination thread identifier to a couple made of an Executor interface and a
thread handle. We need to keep an opened thread handle so that we can wait on
it until it dies.

@subsection pending_registrations_

@link ExecutorsManager::pending_registrations_ pending_registrations_ @endlink
is another map of destination thread identifier, but this one holds on to
synchronization events that we will set once the registration of an executor
for the given destination thread is complete. Since there may be more than one
thread waiting on this signal, and we use the signal only once, we create it as
a manual reset event.

@subsection thread_

Of course, we need to keep a @link ExecutorsManager::thread_ handle @endlink on
our worker thread.

@subsection update_threads_list_gate_

This @link ExecutorsManager::update_threads_list_gate_ event @endlink is used
to wake the worker thread so that it refreshes its list of thread handles that
it waits on. It is set from the RegisterExecutor public method described
@ref RegisterExecutor "above".

@subsection termination_gate_

This other @link ExecutorsManager::termination_gate_ event @endlink is set from
the Terminate public method described @ref Terminate "above" to notify the
worker thread to terminate itself.

@subsection lock_

Since the Executors Manager can be called from many different threads, it must
protect the access to its data (and atomize certain operations) with this @link
ExecutorsManager::lock_ lock @endlink.

*/

#endif  // CEEE_IE_BROKER_EXECUTORS_MANAGER_DOCS_H_

**Quick links:** [browse source and recent revisions]
(https://chromium.googlesource.com/native_client/src/native_client/)

## Browsing the source code

If you just want to look at the source code, you can either [browse]
(https://chromium.googlesource.com/native_client/src/native_client/) it online
or download it with git. Here's how to download the latest version of the source
code:

> `git clone https://chromium.googlesource.com/native_client/src/native_client`

If you use `git clone`, the source code appears under a directory named
`native_client` in the current directory. You can update it using `git pull`.

## Getting buildable source code

If you want to build the latest version of Native Client, follow these steps:

1.  If you don't already have `gclient`, get it by downloading the Chromium
    [depot tools]
    (http://dev.chromium.org/developers/how-tos/install-depot-tools).<br><br>
    <ol><li>If you don't already have <a
    href='http://git-scm.com/downloads'>git</a>, download it.<br><br>
    </li><li>Create a directory to hold the Native Client source code. We'll
    call it <code>$NACL_ROOT</code>.<br><br><b>Important:</b> Make sure the path
    to the directory has no spaces. Examples of good <code>$NACL_ROOT</code>
    values:<br><br><code>/home/me/nativeclient</code>
    <em>(Linux)</em><br><code>/Users/me/nativeclient</code>
    <em>(Mac)</em><br><code>C:\nativeclient</code> <em>(Windows)</em><br><br>
    </li><li>In a shell window, execute the following commands:<br>
    <pre><code>cd $NACL_ROOT<br>
    gclient config https://chromium.googlesource.com/native_client/src/native_client<br>
    </code></pre>
    </li><li>Download the code (this might take a few minutes):<br>
    <pre><code>cd $NACL_ROOT<br>
    gclient sync<br>
    </code></pre>
    </li></ol>

Here's what the <code>gclient</code> steps do:<br> <ul><li><code>gclient
config</code> sets up your working directory, creating a <code>.gclient</code>
file that identifies the structure to pull from the repository.<br>
</li><li><code>gclient sync</code> creates several subdirectories and downloads
the latest Native Client files.</li></ul>

To update the Native Client source code, run <code>gclient sync</code> from
<code>$NACL_ROOT</code> or any of its subdirectories.<br> <br> Once you've
downloaded the source code, you can build Native Client. To do so, you need
Python and a platform-specific development environment. Details on prerequisites
and how to build are in <a
href='http://www.chromium.org/nativeclient/how-tos/build-tcb'>Building Native
Client</a>.

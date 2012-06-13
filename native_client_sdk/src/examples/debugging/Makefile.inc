#
# Setup environment to use SDK's version of the NaCl plugin, instead of the
# one built into Chrome.  This requries launching Chrome which means we must
# be able to find it.  It also means that we must determine which versions
# of the plugin, loader, irt, and any other helpers we must use.
#
HELPER_PATH:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py --helper)
IRTBIN_PATH:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py --irtbin)
LOADER_PATH:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py --loader)
PLUGIN_PATH:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py --plugin)
CHROME_ARGS:=--incognito --no-sandbox
CHROME_ARGS+="--register-pepper-plugins=$(PLUGIN_PATH);application/x-nacl"


ifneq (,$(wildcard $(CHROME_PATH)))
export NACL_DANGEROUS_ENABLE_FILE_ACCESS=1
export NACL_SECURITY_DISABLE=1
export NACL_UNTRUSTED_EXCEPTION_HANDLING=1
export NACL_SEL_LDR=$(LOADER_PATH)
export NACL_IRT_LIBRARY=$(IRTBIN_PATH)
export NACL_SEL_LDR_BOOTSTRAP=$(HELPER_PATH)
endif


TRACE: CHECK_FOR_CHROME all
	$(CHROME_PATH) $(CHROME_ARGS) "localhost:5103/$(PROJECT).html"

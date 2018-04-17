rebuild-headers:
	mkdir -p tools/buildHeaders/build
	cd tools/buildHeaders/build && cmake .. && cmake --build .
	cd include/spirv/unified1 && \
		../../../tools/buildHeaders/build/BuildSpvHeaders -H spirv.core.grammar.json

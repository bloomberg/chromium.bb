;//
;// 
;// File Name:  armSP_FFT_CToC_SC16_Radix4_fs_unsafe_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   7761
;// Last Modified Date:       Wed, 26 Sep 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute a first stage Radix 4 FFT stage for a N point complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)
    
        
        
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}



;// Guarding implementation by the processor name
    
    
    
;// Guarding implementation by the processor name
    
    IF  CortexA8
    
;//Input Registers

pSrc            RN  0
pDst            RN  2
pTwiddle        RN  1
pPingPongBuf    RN  5
subFFTNum       RN  6
subFFTSize      RN  7


;//Output Registers


;//Local Scratch Registers

grpSize         RN  3
setCount        RN  3                  ;// Reuse grpSize as setCount
pointStep       RN  4
outPointStep    RN  4
setStep         RN  8
step1           RN  9
step3           RN  10

;// Neon Registers

dXr0             DN  D0.S16
dXi0             DN  D1.S16
dXr1             DN  D2.S16
dXi1             DN  D3.S16
dXr2             DN  D4.S16
dXi2             DN  D5.S16
dXr3             DN  D6.S16
dXi3             DN  D7.S16
dYr0             DN  D8.S16
dYi0             DN  D9.S16
dYr1             DN  D10.S16
dYi1             DN  D11.S16
dYr2             DN  D12.S16
dYi2             DN  D13.S16
dYr3             DN  D14.S16
dYi3             DN  D15.S16
dZr0             DN  D16.S16
dZi0             DN  D17.S16
dZr1             DN  D18.S16
dZi1             DN  D19.S16
dZr2             DN  D20.S16
dZi2             DN  D21.S16
dZr3             DN  D22.S16
dZi3             DN  D23.S16
qY0              QN  Q4.S16
qY2              QN  Q6.S16
qX0              QN  Q0.S16
qX2              QN  Q2.S16

qY1              QN  Q5.S16
qY3              QN  Q7.S16
qX1              QN  Q1.S16
qX3              QN  Q3.S16
qZ0              QN  Q8.S16
qZ1              QN  Q9.S16

    
        MACRO
        FFTSTAGE $scaled, $inverse, $name
        
        ;// Define stack arguments
        
        MOV     pointStep,subFFTNum
        ;// Update pSubFFTSize and pSubFFTNum regs
        
        
        VLD2    {dXr0,dXi0},[pSrc@128],pointStep          ;//  data[0]
        ;// Note: setCount = subFFTNum/4 (reuse the grpSize reg for setCount)
        LSR     grpSize,subFFTNum,#2  
        MOV     subFFTNum,grpSize
        
               
        ;// pT0+1 increments pT0 by 4 bytes
        ;// pT0+pointStep = increment of 4*pointStep bytes = grpSize bytes
        ;// Note: outPointStep = pointStep for firststage
        VLD2    {dXr1,dXi1},[pSrc@128],pointStep          ;//  data[1]
        
                
        ;// Calculate the step of input data for the next set
        ;//MOV     setStep,pointStep,LSL #1
        MOV     setStep,grpSize,LSL #3
        VLD2    {dXr2,dXi2},[pSrc@128],pointStep          ;//  data[2]
        MOV     step1,setStep
        ADD     setStep,setStep,pointStep             ;// setStep = 3*pointStep
        RSB     setStep,setStep,#16                   ;// setStep = - 3*pointStep+16
        
                
        VLD2    {dXr3,dXi3},[pSrc@128],setStep            ;//  data[3]
        MOV     subFFTSize,#4                         ;// subFFTSize = 1 for the first stage
        
        
        IF  $scaled 
            VHADD    qY0,qX0,qX2             ;// u0
        ELSE
            VADD   qY0,qX0,qX2               ;// u0
        ENDIF
        RSB     step3,pointStep,#0
        
        ;// grp = 0 a special case since all the twiddle factors are 1
        ;// Loop on the sets: 4 sets at a time

grpZeroSetLoop$name        
        
        
        IF $scaled
        
            ;// finish first stage of 4 point FFT 
            
            VHSUB    qY2,qX0,qX2             ;// u1
            SUBS    setCount,setCount,#4                    ;// decrement the set loop counter 
            
            VLD2    {dXr0,dXi0},[pSrc@128],step1          ;//  data[0]
            VHADD    qY1,qX1,qX3             ;// u2
            VLD2    {dXr2,dXi2},[pSrc@128],step3
            VHSUB    qY3,qX1,qX3             ;// u3
            
                        
            
            ;// finish second stage of 4 point FFT 
            
            VLD2    {dXr1,dXi1},[pSrc@128],step1          ;//  data[1]
            VHADD    qZ0,qY0,qY1             ;// y0
            
            VLD2    {dXr3,dXi3},[pSrc@128],setStep 
                        
            
            IF  $inverse 

                VHSUB    dZr3,dYr2,dYi3                  ;// y3
                VHADD    dZi3,dYi2,dYr3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                
                VHSUB    qZ1,qY0,qY1                     ;// y2
                VST2    {dZr3,dZi3},[pDst@128],outPointStep            
                
                VHADD    dZr2,dYr2,dYi3                  ;// y1
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VHSUB    dZi2,dYi2,dYr3
                
                VHADD    qY0,qX0,qX2                     ;// u0 (next loop)
                VST2    {dZr2,dZi2},[pDst@128],setStep     
                
                
            ELSE
            
                VHADD    dZr2,dYr2,dYi3                  ;// y1
                VHSUB    dZi2,dYi2,dYr3
            
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VHSUB    qZ1,qY0,qY1                     ;// y2
                        
                VST2    {dZr2,dZi2},[pDst@128],outPointStep            
                VHSUB    dZr3,dYr2,dYi3                  ;// y3
                VHADD    dZi3,dYi2,dYr3
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VHADD    qY0,qX0,qX2                     ;// u0 (next loop)
                VST2    {dZr3,dZi3},[pDst@128],setStep
                                
            ENDIF
        
        
        ELSE
        
            ;// finish first stage of 4 point FFT 
            
            VSUB    qY2,qX0,qX2             ;// u1
            SUBS    setCount,setCount,#4                    ;// decrement the set loop counter 
            
            VLD2    {dXr0,dXi0},[pSrc@128],step1          ;//  data[0]
            VADD    qY1,qX1,qX3             ;// u2
            VLD2    {dXr2,dXi2},[pSrc@128],step3
            VSUB    qY3,qX1,qX3             ;// u3
            
                        
            
            ;// finish second stage of 4 point FFT 
            
            VLD2    {dXr1,dXi1},[pSrc@128],step1          ;//  data[1]
            VADD    qZ0,qY0,qY1             ;// y0
            
            VLD2    {dXr3,dXi3},[pSrc@128],setStep 
                        
            
            IF  $inverse 

                VSUB    dZr3,dYr2,dYi3                  ;// y3
                VADD    dZi3,dYi2,dYr3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                
                VSUB    qZ1,qY0,qY1                     ;// y2
                VST2    {dZr3,dZi3},[pDst@128],outPointStep            
                
                VADD    dZr2,dYr2,dYi3                  ;// y1
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VSUB    dZi2,dYi2,dYr3
                
                VADD    qY0,qX0,qX2                     ;// u0 (next loop)
                VST2    {dZr2,dZi2},[pDst@128],setStep     
                
                
            ELSE
            
                VADD    dZr2,dYr2,dYi3                  ;// y1
                VSUB    dZi2,dYi2,dYr3
            
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VSUB    qZ1,qY0,qY1                     ;// y2
                        
                VST2    {dZr2,dZi2},[pDst@128],outPointStep            
                VSUB    dZr3,dYr2,dYi3                  ;// y3
                VADD    dZi3,dYi2,dYr3
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VADD    qY0,qX0,qX2                     ;// u0 (next loop)
                VST2    {dZr3,dZi3},[pDst@128],setStep
                                
            ENDIF
                        
                       
        ENDIF
        
        BGT     grpZeroSetLoop$name
        
        
        ;// reset pSrc to pDst for the next stage
        SUB     pSrc,pDst,pointStep                     ;// pDst -= grpSize  
        MOV     pDst,pPingPongBuf
        
        
        MEND

                
        
        M_START armSP_FFTFwd_CToC_SC16_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{FALSE},FWD
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{TRUE},INV
        M_END
 
                
        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{FALSE},FWDSFS
        M_END

                
        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{TRUE},INVSFS
        M_END
    
            
    ENDIF                                                           ;//CortexA8
    
    
     
    END